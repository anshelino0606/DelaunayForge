#pragma once

#include "types.h"

namespace fem::shaderlib {

inline GlobalSession* global_session() {
    static ComPtr<GlobalSession> global_session = nullptr;
    if (!global_session) {
        slang::createGlobalSession(global_session.writeRef());
    }
    return global_session;
}

}