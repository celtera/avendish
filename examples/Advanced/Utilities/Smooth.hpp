#pragma once

struct Smooth {
    struct Ins {
        struct { float value; } in;
        struct { float value; } Type;
        struct { float value; } Amount;
        struct { float value; } Freq_;
        struct { float value; } Cutoff_;
        struct { float value; } Beta_;
        struct { float value; } Continuous;
    } inputs;
    struct {
        struct { float value; } out;
    } outputs;

    void operator()()
    {
        outputs.out.value = inputs.in.value;
    }
};
