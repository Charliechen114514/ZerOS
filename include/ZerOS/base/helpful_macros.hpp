#pragma once

#define DISABLE_COPY(CLASS)       \
    CLASS(const CLASS&) = delete; \
    CLASS& operator=(const CLASS&) = delete

#define DISABLE_COPY_MOVE(CLASS)             \
    CLASS(const CLASS&) = delete;            \
    CLASS& operator=(const CLASS&) = delete; \
    CLASS(CLASS&&) = delete;                 \
    CLASS& operator=(CLASS&&) = delete
