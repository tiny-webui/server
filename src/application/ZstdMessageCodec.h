#pragma once

#include "IMessageCodec.h"
#include <zstd.h>

namespace TUI::Application
{

    class ZstdMessageCodec : public IMessageCodec
    {
    public:
        ZstdMessageCodec();
        ~ZstdMessageCodec() override;
        ZstdMessageCodec(const ZstdMessageCodec&) = delete;
        ZstdMessageCodec& operator=(const ZstdMessageCodec&) = delete;
        ZstdMessageCodec(ZstdMessageCodec&&) = delete;
        ZstdMessageCodec& operator=(ZstdMessageCodec&&) = delete;

        std::vector<std::uint8_t> Encode(const std::vector<std::uint8_t>& data) override;
        std::vector<std::uint8_t> Decode(const std::vector<std::uint8_t>& data) override;
    private:
        ZSTD_CCtx* _cctx{nullptr};
        ZSTD_DCtx* _dctx{nullptr};
    };

}
