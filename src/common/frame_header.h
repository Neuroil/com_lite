//
// Created by Neuroil on 2026/7/21.
//

#ifndef COM_LITE_FRAME_HEADER_H
#define COM_LITE_FRAME_HEADER_H

#include <cstddef>
#include <cstdint>

#include "common/binary_code.h"

/*
 *
 * 帧头布局
 * 0                   4                   8        9
 * ----------------------------------------------------------
 * |     magic_num     |     body_len     | opcode | status |
 * ----------------------------------------------------------
 *        4byte              4byte          1byte    1byte
 *
 */
// 这里初始化逻辑不太好，之后看看有没有更好的
struct FrameHeader {
    uint32_t      magic_num_ = types::magic_head;
    std::uint32_t body_len_  = 0;
    types::Opcode opcode_    = types::Opcode::_init_;
    types::Status status_    = types::Status::_init_;

    // 编译期计算出帧头大小
    static constexpr std::size_t wire_size =
              sizeof(decltype(magic_num_))
            + sizeof(decltype(body_len_))
            + sizeof(decltype(opcode_))
            + sizeof(decltype(status_));

    FrameHeader() = default;

    explicit FrameHeader(types::Opcode opcode, types::Status status) : opcode_(opcode), status_(status) {}

    // op 与 st 均已赋值才可以
    bool is_valid() const { return opcode_ != types::Opcode::_init_ && status_ != types::Status::_init_; }
};

#endif //COM_LITE_FRAME_HEADER_H
