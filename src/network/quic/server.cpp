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

QUIC_STATUS ServerStreamCallback(HQUIC stream, void* ptr, QUIC_STREAM_EVENT* Event) {
    auto ctx = static_cast<QuicServer::Connection*>(ptr);

    switch (Event->Type) {
        case QUIC_STREAM_EVENT_SEND_COMPLETE: {
            // A previous StreamSend call has completed, and the context is being
            // returned back to the app.
            auto req = static_cast<QuicServer::RefCounter*>(Event->SEND_COMPLETE.ClientContext);
            auto c = req->ref.load();
            while (!req->ref.compare_exchange_weak(c, c - 1)) c = req->ref.load();
            if (c == 1) delete req;
            break;
        }
        case QUIC_STREAM_EVENT_RECEIVE:
            // Data was received from the peer on the stream.
            for (uint32_t i = 0; i < Event->RECEIVE.BufferCount; i++) {
                auto& buf = Event->RECEIVE.Buffers[i];
                ctx->received_bytes += buf.Length;
                ctx->recv(buf.Buffer, buf.Length);
            }
            break;
        case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
            // The client gracefully shut down its send direction of the stream.
            printf("[strm][%p] Client stream shutdown\n", stream);
            // ServerSend(Stream);
            break;
        case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
            // The client aborted its send direction of the stream.
            printf("[strm][%p] Client stream aborted\n", stream);
            MsQuic->StreamShutdown(stream, QUIC_STREAM_SHUTDOWN_FLAG_ABORT, 0);
            break;
        case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
            // Both directions of the stream have been shut down and MsQuic is done
            // with the stream. It can now be safely cleaned up.
            printf("[strm][%p] Client stream shutdown complete\n", stream);
            MsQuic->StreamClose(stream);
            break;
        default:
            break;
    }
    return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS ServerConnectionCallback(HQUIC conn, void* ptr, QUIC_CONNECTION_EVENT* event) {
    auto ctx = static_cast<QuicServer::Connection*>(ptr);

    switch (event->Type) {
        case QUIC_CONNECTION_EVENT_CONNECTED: {
            // The handshake has completed for the connection.

            printf("[conn][%p] Client Connected\n", conn);
            MsQuic->ConnectionSendResumptionTicket(conn, QUIC_SEND_RESUMPTION_FLAG_NONE, 0, NULL);

            auto status = MsQuic->StreamOpen(conn, QUIC_STREAM_OPEN_FLAG_0_RTT, ServerStreamCallback, ctx, &ctx->stream);

            if (QUIC_FAILED(status)) {
                printf("[conn][%p] Failed to open stream\n", conn);
                break;
            }

            status = MsQuic->StreamStart(ctx->stream, QUIC_STREAM_START_FLAG_NONE);
            if (QUIC_FAILED(status)) {
                printf("[conn][%p] Failed to start stream\n", conn);
                MsQuic->StreamClose(ctx->stream);
                break;
            }

            ctx->onConnect();
            ctx->server->sync([&] { ctx->server->connections.push_back(ctx); });

            break;
        }

        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:

            // The connection has been shut down by the transport. Generally, this
            // is the expected way for the connection to shut down with this
            // protocol, since we let idle timeout kill the connection.

            if (event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status == QUIC_STATUS_CONNECTION_IDLE) {
                printf("[conn][%p] Client shutdown on idle.\n", conn);
            } else {
                printf("[conn][%p] Shut down by transport, 0x%x\n", conn, event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status);
            }
            break;

        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:

            // The connection was explicitly shut down by the client.

            printf("[conn][%p] Client shutdown by client, 0x%llu\n", conn, (unsigned long long) event->SHUTDOWN_INITIATED_BY_PEER.ErrorCode);
            break;

        case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE: {

            // The connection has completed the shutdown process and is ready to be
            // safely cleaned up.

            printf("[conn][%p] Client shutdown complete\n", conn);

            ctx->server->sync([&] { ctx->server->connections.remove(ctx); });
            ctx->onDisconnect();

            MsQuic->ConnectionClose(conn);
            break;
        }
        case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:

            // The client has created a new stream. The app MUST set the callback handler before returning.

            printf("[strm][%p] Stream started by client\n", event->PEER_STREAM_STARTED.Stream);
            // MsQuic->SetCallbackHandler(Event->PEER_STREAM_STARTED.Stream, ServerStreamCallback, NULL);
            break;

        case QUIC_CONNECTION_EVENT_RESUMED:

            // The connection succeeded in doing a TLS resumption of a previous
            // connection's session.
            // TODO: how to handle this?

            printf("[conn][%p] Connection resumed!\n", conn);
            break;
        case QUIC_CONNECTION_EVENT_IDEAL_PROCESSOR_CHANGED:
            // What does this mean??
            printf("[conn][%p] Ideal processor changed to %u\n", conn, event->IDEAL_PROCESSOR_CHANGED.IdealProcessor);
            break;
        default:
            // TODO: anything else important to handle?
            printf("[conn][%p] Unhandled Event: %u\n", conn, uint8_t(event->Type));
            break;
    }

    return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS ServerListenerCallback(HQUIC listener, void* self, QUIC_LISTENER_EVENT* Event) {
    auto server = static_cast<QuicServer*>(self);
    QUIC_STATUS status = QUIC_STATUS_NOT_SUPPORTED;
    switch (Event->Type) {
        case QUIC_LISTENER_EVENT_NEW_CONNECTION: {
            auto conn = Event->NEW_CONNECTION.Connection;

            auto client = server->client();
            client->conn = conn;
            client->server = server;
            client->stream = nullptr;

            MsQuic->SetCallbackHandler(conn, (void*) ServerConnectionCallback, client);
            status = MsQuic->ConnectionSetConfiguration(Event->NEW_CONNECTION.Connection, Configuration);
            break;
        }
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
    Settings.IdleTimeoutMs = 5000;
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

    printf("[server] open on port: %u\n", port);
    return true;
};

bool QuicServer::stop() {
    if (listener) {
        MsQuic->ListenerClose(listener);
        listener = nullptr;

        return true;
    } else return false;
}

void QuicServer::Connection::disconnect() {
    if (!stream && !conn) return;

    MsQuic->StreamShutdown(stream, QUIC_STREAM_SHUTDOWN_FLAG_GRACEFUL, 0);
    MsQuic->ConnectionShutdown(conn, QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);

    stream = nullptr;
    conn = nullptr;
}

void QuicServer::Connection::send(string_view buffer, bool freeAfterSend, uint8_t compressionMethod) {
    auto req = new SendReq(buffer, 1, freeAfterSend, compressionMethod);
    auto status = MsQuic->StreamSend(stream, req->buffers, 2, QUIC_SEND_FLAG_ALLOW_0_RTT, req);
    if (QUIC_FAILED(status)) delete req;
}

void QuicServer::broadcast(string_view buffer, bool freeAfterSend, bool compress) {
    std::scoped_lock lock(m);
    if (!connections.size()) return;

    auto req = new SendReq(buffer, uint32_t(connections.size()), freeAfterSend, compress);

    for (auto ctx : connections) {
        auto status = MsQuic->StreamSend(ctx->stream, req->buffers, 2, QUIC_SEND_FLAG_ALLOW_0_RTT, req);
        if (QUIC_FAILED(status)) {
            req->ref--;
            printf("Failed to send???\n");
        }
    }
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