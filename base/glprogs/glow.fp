!!ARBfp1.0
# Recovered Xbox Prey glow shader: 7 symmetric pairs, a^i weights,
# no center tap or normalization. Each pass clamps only at RGBA8 output.
# local[0] = a..a^4, local[1] = a^5..a^7, local[2].xy = UV step.
PARAM weights0 = program.local[0];
PARAM weights1 = program.local[1];
PARAM step = program.local[2];
TEMP uv, positive, negative, pair, sum;
MOV sum, 0.0;
MAD uv, step, 1.0, fragment.texcoord[0];
TEX positive, uv, texture[0], 2D;
MAD uv, -step, 1.0, fragment.texcoord[0];
TEX negative, uv, texture[0], 2D;
ADD pair, positive, negative;
MAD sum, pair, weights0.x, sum;
MAD uv, step, 2.0, fragment.texcoord[0];
TEX positive, uv, texture[0], 2D;
MAD uv, -step, 2.0, fragment.texcoord[0];
TEX negative, uv, texture[0], 2D;
ADD pair, positive, negative;
MAD sum, pair, weights0.y, sum;
MAD uv, step, 3.0, fragment.texcoord[0];
TEX positive, uv, texture[0], 2D;
MAD uv, -step, 3.0, fragment.texcoord[0];
TEX negative, uv, texture[0], 2D;
ADD pair, positive, negative;
MAD sum, pair, weights0.z, sum;
MAD uv, step, 4.0, fragment.texcoord[0];
TEX positive, uv, texture[0], 2D;
MAD uv, -step, 4.0, fragment.texcoord[0];
TEX negative, uv, texture[0], 2D;
ADD pair, positive, negative;
MAD sum, pair, weights0.w, sum;
MAD uv, step, 5.0, fragment.texcoord[0];
TEX positive, uv, texture[0], 2D;
MAD uv, -step, 5.0, fragment.texcoord[0];
TEX negative, uv, texture[0], 2D;
ADD pair, positive, negative;
MAD sum, pair, weights1.x, sum;
MAD uv, step, 6.0, fragment.texcoord[0];
TEX positive, uv, texture[0], 2D;
MAD uv, -step, 6.0, fragment.texcoord[0];
TEX negative, uv, texture[0], 2D;
ADD pair, positive, negative;
MAD sum, pair, weights1.y, sum;
MAD uv, step, 7.0, fragment.texcoord[0];
TEX positive, uv, texture[0], 2D;
MAD uv, -step, 7.0, fragment.texcoord[0];
TEX negative, uv, texture[0], 2D;
ADD pair, positive, negative;
MAD sum, pair, weights1.z, sum;
MOV result.color, sum;
END
