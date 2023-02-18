function complex(real, imaginary) {
    this.r = real;
    this.i = imaginary;
}

function cadd(a, b) {
    return new complex(a.r + b.r, a.i + b.i);
}

function csub(a, b) {
    return new complex(a.r - b.r, a.i - b.i);
}

function cmult(a, b) {
    return new complex(a.r * b.r - a.i * b.i,
                       a.r * b.i + a.i * b.r);
}

function cneg(a) {
    return new complex(-a.r, -a.i);
}

function cpow(x, pow) {
    var result = new complex(1, 0);
    for (var i = 0; i < pow; i++) {
        result = cmult(result, x);
    }
    return result;
}

function cdiv(a, b) {
    const denom = b.r * b.r + b.i * b.i;
    return new complex((a.r * b.r + a.i * b.i) / denom,
                       (a.i * b.r - a.r * b.i) / denom);
}

function cnorm2(a) {
    return a.r * a.r + a.i * a.i;
}

function polynomial(coefficients) {
    this.coefficients = coefficients;
    this.zeros = null;
    this.deriv_ = null;
}

function pmult(a, b) {
    const A = a.coefficients.length;
    const B = b.coefficients.length;
    if (A == 0 || B == 0) {
        return new polynomial([]);
    }
    result = Array(A + B - 1).fill(new complex(0, 0));
    for (var i = 0; i < A; i++) {
        for (var j = 0; j < B; j++) {
            const k = i + j;
            result[k] = cadd(result[k], cmult(a.coefficients[i],
                                              b.coefficients[j]));
        }
    }
    return new polynomial(result);
}

function polynomial_from_zeros(zeros) {
    if (zeros.length == 0) {
        return new polynomial([]);
    }
    result = new polynomial([cneg(zeros[0]), new complex(1, 0)]);
    for (var i = 1; i < zeros.length; i++) {
        var factor = new polynomial([cneg(zeros[i]), new complex(1, 0)]);
        result = pmult(result, factor);
    }
    result.zeros = zeros;
    return result;
}

polynomial.prototype.derivative = function() {
    if (this.deriv_ === null) {
        var result = [];
        for (var i = 1; i < this.coefficients.length; i++) {
            result.push(cmult(new complex(i, 0), this.coefficients[i]));
        }
        this.deriv_ = new polynomial(result);
    }
    return this.deriv_;
}

polynomial.prototype.at = function(x) {
    if (this.zeros !== null) {
        var result = csub(x, this.zeros[0]);
        for (var i = 1; i < this.zeros.length; i++) {
            result = cmult(result, csub(x, this.zeros[i]));
        }
        return result;
    }
    if (this.coefficients.length == 0) {
        return new complex(0, 0);
    }
    var result = this.coefficients[0];
    var x_pow = x;
    var i = 1;
    for (; i < this.coefficients.length - 1; i++) {
        result = cadd(result, cmult(this.coefficients[i], x_pow));
        x_pow = cmult(x_pow, x);
    }
    result = cadd(result, cmult(this.coefficients[i], x_pow));
    return result;
}

polynomial.prototype.to_string = function() {
    if (this.coefficients.length == 0) {
        return "0";
    }
    var terms = this.coefficients.map((c, index) => {
        const c_str = "(" + c.r + "+" + c.i + "i)";
        const var_str = index == 0 ? "" : "x^" + index;
        return c_str + var_str;
    });
    return terms.join(" + ");
}

function newton(polynomial, guess, iterations) {
    for (var i = 0; i < iterations; i++) {
        guess = csub(guess, cdiv(polynomial.at(guess),
                                 polynomial.derivative().at(guess)));
    }
    return guess;
}

function closest_zero(c, zeros) {
    var closest = 0;
    var closest_d2 = cnorm2(csub(c, zeros[0]));
    for (var i = 1; i < zeros.length; i++) {
        var d2 = cnorm2(csub(c, zeros[i]));
        if (d2 < closest_d2) {
            closest_d2 = d2;
            closest = i;
        }
    }
    return closest;
}

function main() {
    const canvas = document.getElementById("fractal-canvas");
    const ctx = canvas.getContext("2d");

    const origin = new complex(0, 0);
    const r_rng = 5;
    const i_rng = r_rng / canvas.width * canvas.height;
    const r_min = origin.r - r_rng / 2;
    const r_max = origin.r + r_rng / 2;
    const i_min = origin.i - i_rng / 2;
    const i_max = origin.i + i_rng / 2;

    const p = polynomial_from_zeros([
        new complex(1, 0),
        new complex(-0.5, 0.86602540378),
        new complex(-0.5, -0.86602540378)
    ]);
    console.log(p.to_string());

    for (var x = 0; x < canvas.width; x++) {
        const r = x / canvas.width * (r_max - r_min) + r_min;
        for (var y = 0; y < canvas.height; y++) {
            const i = y / canvas.height * (i_max - i_min) + i_min;
            const result = newton(p, new complex(r, i), 100);
            const zero_index = closest_zero(result, p.zeros);
            if (zero_index == 0) {
                ctx.fillStyle = "rgb(255,0,0)";
            } else if (zero_index == 1) {
                ctx.fillStyle = "rgb(0,255,0)";
            } else if (zero_index == 2) {
                ctx.fillStyle = "rgb(0,0,255)";
            }
            ctx.fillRect(x, y, 1, 1);
        }
    }    
}

addEventListener("load", (event) => {main();});
