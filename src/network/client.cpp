#include <stdio.h>
#include <stdlib.h>
#include "client.hpp"

// The QUIC API/function table returned from MsQuicOpen. It contains all the
// functions called by the app to interact with MsQuic.
const QUIC_API_TABLE* MsQuic;

// The QUIC handle to the registration object. This is the top level API object
// that represents the execution context for all work done by MsQuic on behalf
// of the app.
HQUIC Registration;

// The QUIC handle to the configuration object. This object abstracts the
// connection configuration. This includes TLS configuration and any other
// QUIC layer settings.
HQUIC Configuration;

QUIC_STATUS ClientStreamCallback(HQUIC stream, void* self, QUIC_STREAM_EVENT* Event) {
    auto client = static_cast<QuicClient*>(self);

    switch (Event->Type) {
        case QUIC_STREAM_EVENT_START_COMPLETE:
            printf("[strm][%p] Stream started\n", stream);
            break;
        case QUIC_STREAM_EVENT_SEND_COMPLETE:
            // A previous StreamSend call has completed, and the context is being
            // returned back to the app.
            delete static_cast<QuicClient::SendReq*>(Event->SEND_COMPLETE.ClientContext);
            printf("[strm][%p] Data sent\n", stream);
            break;
        case QUIC_STREAM_EVENT_RECEIVE:
            // Data was received from the peer on the stream.
            printf("[strm][%p] Received %u buffers\n", stream, Event->RECEIVE.BufferCount);
            for (uint32_t i = 0; i < Event->RECEIVE.BufferCount; i++) {
                auto& buf = Event->RECEIVE.Buffers[i];
                printf("Buffers[%u] = %.*s\n", i, buf.Length, buf.Buffer);
            }
            break;
        case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
            // Server gracefully shut down its send direction of the stream.
            printf("[strm][%p] Peer aborted\n", stream);
            break;
        case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
            // Server aborted its send direction of the stream.
            printf("[strm][%p] Peer shut down\n", stream);
            break;
        case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
            // Both directions of the stream have been shut down and MsQuic is done
            // with the stream. It can now be safely cleaned up.
            printf("[strm][%p] Stream shutdown complete\n", stream);
            MsQuic->StreamClose(stream);
            break;
        default:
            break;
    }
    return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS ClientConnectionCallback(HQUIC conn, void* self, QUIC_CONNECTION_EVENT* Event) {
    auto client = static_cast<QuicClient*>(self);

    switch (Event->Type) {
        case QUIC_CONNECTION_EVENT_CONNECTED:
            // The handshake has completed for the connection.
            printf("[conn][%p] Connected\n", conn);
            // ClientSend(conn);
            break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
            // The connection has been shut down by the transport. Generally, this
            // is the expected way for the connection to shut down with this
            // protocol, since we let idle timeout kill the connection.
            if (Event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status == QUIC_STATUS_CONNECTION_IDLE) {
                printf("[conn][%p] Successfully shut down on idle.\n", conn);
            } else {
                printf("[conn][%p] Shut down by transport, 0x%x\n", conn, Event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status);
            }
            break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
            // The connection was explicitly shut down by the peer.
            printf("[conn][%p] Shutdown by server, 0x%llu\n", conn, (unsigned long long)Event->SHUTDOWN_INITIATED_BY_PEER.ErrorCode);
            break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
            // The connection has completed the shutdown process and is ready to be
            // safely cleaned up.
            printf("[conn][%p] Shutdown complete\n", conn);
            if (!Event->SHUTDOWN_COMPLETE.AppCloseInProgress) {
                MsQuic->ConnectionClose(conn);
            }
            break;
        case QUIC_CONNECTION_EVENT_RESUMPTION_TICKET_RECEIVED:
            // A resumption ticket (also called New Session Ticket or NST) was
            // received from the server.
            printf("[conn][%p] Resumption ticket received (%u bytes):\n", conn, Event->RESUMPTION_TICKET_RECEIVED.ResumptionTicketLength);
            for (uint32_t i = 0; i < Event->RESUMPTION_TICKET_RECEIVED.ResumptionTicketLength; i++) {
                printf("%.2X", (uint8_t)Event->RESUMPTION_TICKET_RECEIVED.ResumptionTicket[i]);
            }
            printf("\n");
            break;
        case QUIC_CONNECTION_EVENT_STREAMS_AVAILABLE:
            printf("Available stream count: bi = %u, uni = %u\n",
                Event->STREAMS_AVAILABLE.BidirectionalCount,
                Event->STREAMS_AVAILABLE.UnidirectionalCount);
            break;
        case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
            // The server has created a new stream. The app MUST set the callback handler before returning.
            printf("[strm][%p] Stream started by server \n", Event->PEER_STREAM_STARTED.Stream);
            client->stream = Event->PEER_STREAM_STARTED.Stream;
            MsQuic->SetCallbackHandler(Event->PEER_STREAM_STARTED.Stream, (void*) ClientStreamCallback, client);
            break;
        default:
            // TODO: anything else important to handle?
            printf("[conn][%p] Unhandled Event: %u\n", conn, uint8_t(Event->Type));
            break;
    }
    return QUIC_STATUS_SUCCESS;
}

int QuicClient::init(bool insecure) {
    QUIC_STATUS status = QUIC_STATUS_SUCCESS;

    status = MsQuicOpen(&MsQuic);
    if (QUIC_FAILED(status)) {
        printf("MsQuicOpen failed , 0x%x!\n", status);
        return 1;
    }

    // The (optional) registration configuration for the app. This sets a name for
    // the app (used for persistent storage and for debugging). It also configures
    // the execution profile, using the default "low latency" profile.
    const QUIC_REGISTRATION_CONFIG RegConfig = { "physx-quic-reg", QUIC_EXECUTION_PROFILE_LOW_LATENCY };

    status = MsQuic->RegistrationOpen(&RegConfig, &Registration);

    if (QUIC_FAILED(status)) {
        printf("RegistrationOpen failed, 0x%x!\n", status);
        return 1;
    }

    QUIC_SETTINGS Settings = { 0 };

    // Configures the client's idle timeout.
    Settings.IdleTimeoutMs = 5000;
    Settings.IsSet.IdleTimeoutMs = TRUE;

    Settings.PeerBidiStreamCount = 1;
    Settings.IsSet.PeerBidiStreamCount = TRUE;

    // Configures a default client configuration, optionally disabling
    // server certificate validation.
    QUIC_CREDENTIAL_CONFIG CredConfig;
    memset(&CredConfig, 0, sizeof(CredConfig));
    CredConfig.Type = QUIC_CREDENTIAL_TYPE_NONE;
    CredConfig.Flags = QUIC_CREDENTIAL_FLAG_CLIENT;
    
    if (insecure) {
        CredConfig.Flags |= QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;
    }

    // The protocol name used in the Application Layer Protocol Negotiation (ALPN).
    const string PROTO_NAME = "physx-quic";
    const QUIC_BUFFER ALPN({ (uint32_t) PROTO_NAME.length(), (uint8_t*) PROTO_NAME.data() });

    // Allocate/initialize the configuration object, with the configured ALPN
    // and settings.

    status = MsQuic->ConfigurationOpen(Registration, &ALPN, 1, &Settings, sizeof(Settings), NULL, &Configuration);
    if (QUIC_FAILED(status)) {
        printf("ConfigurationOpen failed, 0x%x!\n", status);
        return 1;
    }

    // Loads the TLS credential part of the configuration. This is required even
    // on client side, to indicate if a certificate is required or not.

    status = MsQuic->ConfigurationLoadCredential(Configuration, &CredConfig);
    if (QUIC_FAILED(status)) {
        printf("ConfigurationLoadCredential failed, 0x%x!\n", status);
        return 1;
    }

    return 0;
}

bool QuicClient::connect(string host, uint16_t port) {
    QUIC_STATUS status = QUIC_STATUS_SUCCESS;

    // Allocate a new connection object.
    status = MsQuic->ConnectionOpen(Registration, ClientConnectionCallback, this, &conn);
    if (QUIC_FAILED(status)) {
        printf("ConnectionOpen failed, 0x%x!\n", status);
        return false;
    }

    // Use resume ticket
    if (false) {
        uint8_t ResumptionTicket[1024];
        uint16_t TicketLength = 0;

        status = MsQuic->SetParam(conn, QUIC_PARAM_LEVEL_CONNECTION, QUIC_PARAM_CONN_RESUMPTION_TICKET, TicketLength, ResumptionTicket);
        if (QUIC_FAILED(status)) {
            printf("SetParam(QUIC_PARAM_CONN_RESUMPTION_TICKET) failed, 0x%x!\n", status);
            return 1;
        }
    }

    printf("[conn][%p] Connecting...\n", conn);

    MsQuic->ConnectionStart(conn, Configuration, QUIC_ADDRESS_FAMILY_UNSPEC, host.c_str(), port);
    if (QUIC_FAILED(status)) {
        printf("ConnectionStart failed, 0x%x!\n", status);
        return false;
    }

    return true;
}

bool QuicClient::disconnect() {
    if (conn) {
        MsQuic->ConnectionClose(conn);
        conn = nullptr;
        return true;
    } else return false;
}

bool QuicClient::send(string_view buffer, bool freeAfterSend) {
    if (!stream) return false;
    auto buffers = new QUIC_BUFFER[1];
    buffers[0].Buffer = (uint8_t*) buffer.data();
    buffers[0].Length = buffer.size();
    auto req = new SendReq{ 1, buffers, freeAfterSend };
    auto status = MsQuic->StreamSend(stream, buffers, 1, QUIC_SEND_FLAG_ALLOW_0_RTT, req);
    return !QUIC_FAILED(status);
}

void QuicClient::cleanup() {
    if (MsQuic) {
        if (Configuration) {
            MsQuic->ConfigurationClose(Configuration);
        }
        // This will block until all outstanding child objects have been closed.
        if (Registration) {
            MsQuic->RegistrationClose(Registration);
        }
        MsQuicClose(MsQuic);
    }
}