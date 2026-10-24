// SPDX-License-Identifier: GPL-3.0-or-later
#include "game/game/q_shared.h"
#include "qcommon/qcommon.h"
#include <cstdio>
#include <stdexcept>
#include <vector>

extern cvar_t *showpackets, *showdrop, *qport;
cvar_t *cl_shownet;
void Com_Memset(void *destination, int value, size_t size) {
    memset(destination, value, size);
}
void Com_Memcpy(void *destination, const void *source, size_t size) {
    memcpy(destination, source, size);
}
void QDECL Com_Printf(const char *, ...) {
}
void QDECL Com_DPrintf(const char *, ...) {
}
void QDECL Com_Error(int, const char *message, ...) {
    throw std::runtime_error(message);
}
void Sys_SendPacket(int, const void *, netadr_t) {
    throw std::runtime_error("Unexpected UDP");
}
cvar_t *Cvar_Get(const char *, const char *, int) {
    static cvar_t value{};
    return &value;
}
qboolean Sys_StringToAdr(const char *, netadr_t *) {
    return qfalse;
}

int main() {
    try {
        Swap_Init();
        static cvar_t zero{};
        showpackets = showdrop = qport = &zero;
        for (int length : {1200, 1300, 2600, 7061, 32000}) {
            netadr_t address{};
            address.type = NA_LOOPBACK;
            netchan_t sender{}, receiver{};
            Netchan_Setup(NS_SERVER, &sender, address, 0);
            Netchan_Setup(NS_CLIENT, &receiver, address, 0);
            std::vector<byte> original(length);
            for (int index = 0; index < length; ++index) {
                original[index] = static_cast<byte>((index * 17 + index / 100) & 255);
            }
            byte bytes[MAX_MSGLEN + 4];
            msg_t message;
            MSG_Init(&message, bytes, sizeof(bytes));
            Netchan_Transmit(&sender, length, original.data());
            bool complete = false;
            do {
                netadr_t from;
                while (NET_GetLoopPacket(NS_CLIENT, &from, &message)) {
                    if (Netchan_Process(&receiver, &message)) {
                        if (message.cursize != length + 4 ||
                            memcmp(message.data + 4, original.data(), length)) {
                            throw std::runtime_error("Fragmented payload was corrupted");
                        }
                        complete = true;
                    }
                }
                if (sender.unsentFragments) {
                    Netchan_TransmitNextFragment(&sender);
                } else {
                    break;
                }
            } while (!complete);
            netadr_t from;
            while (NET_GetLoopPacket(NS_CLIENT, &from, &message)) {
                if (Netchan_Process(&receiver, &message)) {
                    if (message.cursize != length + 4 ||
                        memcmp(message.data + 4, original.data(), length)) {
                        throw std::runtime_error("Final fragmented payload was corrupted");
                    }
                    complete = true;
                }
            }
            if (!complete) {
                throw std::runtime_error("No reassembled message");
            }
            if (receiver.incomingSequence != 1 || sender.outgoingSequence != 2) {
                throw std::runtime_error("Fragment completion did not advance the sequence");
            }
            MSG_InitOOB(&message, bytes, sizeof(bytes));
            MSG_WriteLong(&message, 1);
            MSG_WriteByte(&message, 123);
            if (Netchan_Process(&receiver, &message)) {
                throw std::runtime_error("Completed fragment sequence accepted a replay");
            }
        }
        // The reconstructed packet also contains a four-byte sequence header.
        // Physical fragments fit this buffer, but the complete payload does not.
        netadr_t address{};
        address.type = NA_LOOPBACK;
        netchan_t sender{}, receiver{};
        Netchan_Setup(NS_SERVER, &sender, address, 0);
        Netchan_Setup(NS_CLIENT, &receiver, address, 0);
        byte payload[2600]{};
        byte bytes[2603]{};
        msg_t message;
        MSG_Init(&message, bytes, sizeof(bytes));
        Netchan_Transmit(&sender, sizeof(payload), payload);
        do {
            netadr_t from;
            while (NET_GetLoopPacket(NS_CLIENT, &from, &message)) {
                if (Netchan_Process(&receiver, &message)) {
                    throw std::runtime_error("Reassembled sequence header exceeded capacity");
                }
            }
            if (!sender.unsentFragments) {
                break;
            }
            Netchan_TransmitNextFragment(&sender);
        } while (true);
        std::puts("Network fragment payload checks passed.");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
