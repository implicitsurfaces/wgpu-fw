struct complex {
    real: f32,
    imag: f32,
};

fn c_add(a: complex, b: complex) -> complex {
    return complex(
        real: a.real + b.real,
        imag: a.imag + b.imag,
    );
}

fn c_sub(a: complex, b: complex) -> complex {
    return complex(
        real: a.real - b.real,
        imag: a.imag - b.imag,
    );
}

fn c_mul(a: complex, b: complex) -> complex {
    return complex(
        real: a.real * b.real - a.imag * b.imag,
        imag: a.real * b.imag + a.imag * b.real,
    );
}

fn c_div(a: complex, b: complex) -> complex {
    let denom: f32 = b.real * b.real + b.imag * b.imag;
    return complex(
        real: (a.real * b.real + a.imag * b.imag) / denom,
        imag: (a.imag * b.real - a.real * b.imag) / denom,
    );
}

fn c_abs(a: complex) -> f32 {
    return sqrt(a.real * a.real + a.imag * a.imag);
}

fn c_arg(a: complex) -> f32 {
    return atan2(a.imag, a.real);
}

fn c_neg(a: complex) -> complex {
    return complex(
        real: -a.real,
        imag: -a.imag,
    );
}

fn c_mod2(a: complex) -> f32 {
    return a.real * a.real + a.imag * a.imag;
}

fn c_conj(a: complex) -> complex {
    return complex(
        real:  a.real,
        imag: -a.imag,
    );
}

fn c_exp(a: complex) -> complex {
    let exp_real: f32 = exp(a.real);
    return complex(
        real: exp_real * cos(a.imag),
        imag: exp_real * sin(a.imag),
    );
}
