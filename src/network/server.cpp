#include <stdio.h>
#include <stdlib.h>
#include "server.hpp"

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

typedef struct QUIC_CREDENTIAL_CONFIG_HELPER {
    QUIC_CREDENTIAL_CONFIG CredConfig;
    union {
        QUIC_CERTIFICATE_HASH CertHash;
        QUIC_CERTIFICATE_HASH_STORE CertHashStore;
        QUIC_CERTIFICATE_FILE CertFile;
        QUIC_CERTIFICATE_FILE_PROTECTED CertFileProtected;
    }; // ??? why union
} QUIC_CREDENTIAL_CONFIG_HELPER;

// Terrifying microsoft dev code

QUIC_STATUS ServerStreamCallback(HQUIC stream, void* self, QUIC_STREAM_EVENT* Event) {
    switch (Event->Type) {
    case QUIC_STREAM_EVENT_SEND_COMPLETE:
        //
        // A previous StreamSend call has completed, and the context is being
        // returned back to the app.
        //
        free(Event->SEND_COMPLETE.ClientContext);
        printf("[strm][%p] Data sent\n", stream);
        break;
    case QUIC_STREAM_EVENT_RECEIVE:
        //
        // Data was received from the peer on the stream.
        //
        printf("[strm][%p] Data received\n", stream);
        break;
    case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
        //
        // The peer gracefully shut down its send direction of the stream.
        //
        printf("[strm][%p] Peer shut down\n", stream);
        // ServerSend(Stream);
        break;
    case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
        //
        // The peer aborted its send direction of the stream.
        //
        printf("[strm][%p] Peer aborted\n", stream);
        MsQuic->StreamShutdown(stream, QUIC_STREAM_SHUTDOWN_FLAG_ABORT, 0);
        break;
    case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
        //
        // Both directions of the stream have been shut down and MsQuic is done
        // with the stream. It can now be safely cleaned up.
        //
        printf("[strm][%p] All done\n", stream);
        MsQuic->StreamClose(stream);
        break;
    default:
        break;
    }
    return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS ServerConnectionCallback(HQUIC conn, void* self, QUIC_CONNECTION_EVENT* Event) {
    switch (Event->Type) {
        case QUIC_CONNECTION_EVENT_CONNECTED:
            //
            // The handshake has completed for the connection.
            //
            printf("[conn][%p] Connected\n", conn);
            MsQuic->ConnectionSendResumptionTicket(conn, QUIC_SEND_RESUMPTION_FLAG_NONE, 0, NULL);
            break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
            //
            // The connection has been shut down by the transport. Generally, this
            // is the expected way for the connection to shut down with this
            // protocol, since we let idle timeout kill the connection.
            //
            if (Event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status == QUIC_STATUS_CONNECTION_IDLE) {
                printf("[conn][%p] Successfully shut down on idle.\n", conn);
            } else {
                printf("[conn][%p] Shut down by transport, 0x%x\n", conn, Event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status);
            }
            break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
            //
            // The connection was explicitly shut down by the peer.
            //
            printf("[conn][%p] Shut down by peer, 0x%llu\n", conn, (unsigned long long) Event->SHUTDOWN_INITIATED_BY_PEER.ErrorCode);
            break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
            //
            // The connection has completed the shutdown process and is ready to be
            // safely cleaned up.
            //
            printf("[conn][%p] All done\n", conn);
            MsQuic->ConnectionClose(conn);
            break;
        case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
            //
            // The peer has started/created a new stream. The app MUST set the
            // callback handler before returning.
            //
            printf("[strm][%p] Peer started\n", Event->PEER_STREAM_STARTED.Stream);
            MsQuic->SetCallbackHandler(Event->PEER_STREAM_STARTED.Stream, ServerStreamCallback, NULL);
            break;
        case QUIC_CONNECTION_EVENT_RESUMED:
            //
            // The connection succeeded in doing a TLS resumption of a previous
            // connection's session.
            //
            printf("[conn][%p] Connection resumed!\n", conn);
            break;
        default:
            break;
    }
    return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS ServerListenerCallback(HQUIC listener, void* self, QUIC_LISTENER_EVENT* Event) {
    QUIC_STATUS status = QUIC_STATUS_NOT_SUPPORTED;
    switch (Event->Type) {
    case QUIC_LISTENER_EVENT_NEW_CONNECTION:
        MsQuic->SetCallbackHandler(Event->NEW_CONNECTION.Connection, ServerConnectionCallback, self);
        status = MsQuic->ConnectionSetConfiguration(Event->NEW_CONNECTION.Connection, Configuration);
        break;
    }
    return status;
}

// The protocol name used in the Application Layer Protocol Negotiation (ALPN).
const string PROTO_NAME = "physx-quic";
const QUIC_BUFFER ALPN({ (uint32_t) PROTO_NAME.length(), (uint8_t*) PROTO_NAME.data() });

int QuicServer::init() {
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

    // Configures the server's idle timeout.
    Settings.IdleTimeoutMs = 1000;
    Settings.IsSet.IdleTimeoutMs = TRUE;

    // Configures the server's resumption level to allow for resumption and
    // 0-RTT.
    Settings.ServerResumptionLevel = QUIC_SERVER_RESUME_AND_ZERORTT;
    Settings.IsSet.ServerResumptionLevel = TRUE;

    // Configures the server's settings to allow for the peer to open a single
    // bidirectional stream. By default connections are not configured to allow
    // any streams from the peer.
    Settings.PeerBidiStreamCount = 1;
    Settings.IsSet.PeerBidiStreamCount = TRUE;

    QUIC_CREDENTIAL_CONFIG_HELPER Config;
    memset(&Config, 0, sizeof(Config));
    Config.CredConfig.Flags = QUIC_CREDENTIAL_FLAG_NONE;
    Config.CertFile.CertificateFile = "server.cert";
    Config.CertFile.PrivateKeyFile = "server.key";
    Config.CredConfig.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_FILE;
    Config.CredConfig.CertificateFile = &Config.CertFile;

    // Allocate/initialize the configuration object, with the configured ALPN  settings.
    status = MsQuic->ConfigurationOpen(Registration, &ALPN, 1, &Settings, sizeof(Settings), NULL, &Configuration);
    if (QUIC_FAILED(status)) {
        printf("QUIC ConfigurationOpen failed, 0x%x!\n", status);
        return 1;
    }

    // Loads the TLS credential part of the configuration.
    status = MsQuic->ConfigurationLoadCredential(Configuration, &Config.CredConfig);
    if (QUIC_FAILED(status)) {
        printf("QUIC ConfigurationLoadCredential failed, 0x%x!\n", status);
        return 1;
    }

    return 0;
}

bool QuicServer::listen(uint16_t port) {
    QUIC_STATUS status;

    // Configures the address used for the listener to listen on all IP
    // addresses and the given UDP port.
    QUIC_ADDR addr = { 0 };
    QuicAddrSetFamily(&addr, QUIC_ADDRESS_FAMILY_UNSPEC);
    QuicAddrSetPort(&addr, port);

    // Create / allocate a new listener object.
    status = MsQuic->ListenerOpen(Registration, ServerListenerCallback, this, &listener);

    if (QUIC_FAILED(status)) {
        printf("QUIC ListenerOpen failed, 0x%x!\n", status);
        return false;
    }

    // Starts listening for incoming connections.
    status = MsQuic->ListenerStart(listener, &ALPN, 1, &addr);

    if (QUIC_FAILED(status)) {
        printf("QUIC ListenerStart failed, 0x%x!\n", status);
        return false;
    }

    printf("Server listening on port: %u\n", port);
    return true;
};

bool QuicServer::stop() {
    if (listener) {
        MsQuic->ListenerClose(listener);
        listener = nullptr;

        return true;
    } else return false;
}

void QuicServer::cleanup() {
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