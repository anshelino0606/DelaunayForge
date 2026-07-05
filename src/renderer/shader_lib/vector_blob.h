#pragma once

#include <slang.h>
#include <slang-com-ptr.h>
#include <vector>

namespace fem::shaderlib {

class VectorBlob : public slang::IBlob {
public:
    VectorBlob(std::vector<uint8_t>&& data) : m_data(std::move(data)) {}

    virtual SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(SlangUUID const& guid, void** outInterface) override {
        if (guid == ISlangUnknown::getTypeGuid() || guid == slang::IBlob::getTypeGuid()) {
            *outInterface = static_cast<slang::IBlob*>(this);
            return SLANG_OK;
        }
        return SLANG_E_NO_INTERFACE;
    }
    virtual SLANG_NO_THROW uint32_t SLANG_MCALL addRef() override { return 1; }
    virtual SLANG_NO_THROW uint32_t SLANG_MCALL release() override { return 1; }

    virtual SLANG_NO_THROW void const* SLANG_MCALL getBufferPointer() override { return m_data.data(); }
    virtual SLANG_NO_THROW size_t SLANG_MCALL getBufferSize() override { return m_data.size(); }

private:
    std::vector<uint8_t> m_data;
};

}