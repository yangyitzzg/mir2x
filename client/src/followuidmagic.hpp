/*
 * =====================================================================================
 *
 *       Filename: followuidmagic.hpp
 *        Created: 12/07/2020 21:19:44
 *    Description:
 *
 *        Version: 1.0
 *       Revision: none
 *       Compiler: gcc
 *
 *         Author: ANHONG
 *          Email: anhonghe@gmail.com
 *   Organization: USTC
 *
 * =====================================================================================
 */

#pragma once
#include <cstdint>
#include "sysconst.hpp"
#include "fflerror.hpp"
#include "magicbase.hpp"

class ProcessRun;
class FollowUIDMagic: public MagicBase
{
    protected:
        int m_x;
        int m_y;
        int m_flyDirIndex;

    protected:
        int m_moveSpeed;

    protected:
        uint64_t m_uid;
        ProcessRun *m_process;

    private:
        int m_lastLDistance2 = INT_MAX;
        std::optional<std::tuple<int, int>> m_lastFlyOff;

    public:
        FollowUIDMagic(
                const char8_t *, // magicName
                const char8_t *, // stageName
                int,             // x
                int,             // y
                int,             // gfxDirIndex, can be [0, m_gfxEntry.gfxDirIndex)
                int,             // flyDirIndex, flying direction when targetUID is unavailable and lastFlyOff is not set, always use [0, 16)
                int,             // moveSpeed
                uint64_t,        // targetUID
                ProcessRun *);   // ProcessRun

    public:
        bool done() const override;

    public:
        bool update(double) override;

    public:
        virtual void drawViewOff(int, int, uint32_t) const;

    public:
        virtual uint32_t frameTexID() const;

    public:
        std::tuple<int, int> targetOff() const;

    public:
        std::tuple<int, int> targetPLoc() const
        {
            const auto [txOff, tyOff] = targetOff();
            return
            {
                m_x + txOff,
                m_y + tyOff,
            };
        }
};
