/* SPDX-License-Identifier: GPL-3.0-only
 * VOID engine: PARCHE — synthetic membrane and resonant body.
 * Uses the existing MIT physical-model DSP primitives with its own controls
 * and state. Designed as a playable texture, not a drum-kit replacement.
 */
#define PARCHE_POLY 3u
typedef struct {
    px_modal_t membrane;
    uint32_t age;
} parche_slot_t;

ENG_MEM_FITS("PARCHE", sizeof(parche_slot_t) * PARCHE_POLY);

static parche_slot_t *parche_slot_of(track_t *t, voice_t *v)
{
    uint32_t i;
    if (t < &trk[0] || t >= &trk[NPART]) return 0;
    i = (uint32_t)(v - t->v);
    return i < PARCHE_POLY ? (parche_slot_t *)ENG_MEM(t - trk) + i : 0;
}

static void parche_note_on(track_t *t, voice_t *v)
{
    parche_slot_t *s = parche_slot_of(t, v);
    if (!s) return;
    if ((!v->env && !v->env_out) || !s->age) {
        uint32_t *w = (uint32_t *)&s->membrane;
        uint32_t i;
        for (i = 0; i < sizeof s->membrane / 4u; ++i) w[i] = 0;
        s->membrane.rng = (uint32_t)(v->age * 2654435761u) | 1u;
    }
    s->membrane.trig = 1;
    s->age = v->age + 1u;
}

static void parche_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    parche_slot_t *s = parche_slot_of(t, v);
    px_modal_blk_t b;
    int32_t y[CTL], ax[CTL];
    uint32_t i;
    int32_t harm, brit, damp, acc, pos, bend, body_gain, snap;
    if (!s) return;
    if (n > CTL) n = CTL;
    harm = clamp(p[P_E0] * 516, 0, 65536);
    brit = clamp(p[P_E1] * 516 + m->cutoff * 2, 0, 65536);
    damp = clamp(p[P_E2] * 516, 0, 65536);
    pos = clamp(p[P_E3], 0, 127);
    acc = clamp(p[P_E4] * (v->vel ? v->vel : 1u) * 4, 0, 65536);
    bend = clamp(p[P_E5] * 6192, 0, 786432);
    body_gain = clamp(18000 + p[P_E6] * 350, 8000, 62000);
    snap = clamp(p[P_E7] * 258, 0, 32767);
    /* Excitation is block-based; no expensive per-sample setup. */
    px_memb_block(&b, &s->membrane, m->inc, harm, brit, damp, acc, pos * 516, bend);
    px_modal_run(&b, &s->membrane, y, ax, n);
    for (i = 0; i < n; ++i) {
        int32_t strike = px_m(ax[i], snap, 15);
        int32_t sample = px_m(y[i], body_gain, 20) + px_m(strike, pos < 64 ? 7000 : 2500, 15);
        int32_t a = sample < 0 ? -sample : sample;
        if (a > 22000) {
            a = 22000 + (softclip(clamp(a - 22000, 0, 1 << 20) * 2) >> 1);
            sample = sample < 0 ? -a : a;
        }
        out[i] += voice_amp(sample, m, i) << 1;
    }
}

static const preset_t PARCHE_PRESETS[] = {
    {"PIEL OPACA", {64, 34, 82, 30, 90, 8, 52, 28}, {0, 100, 127, 64}, 0, 0, FX(0, 8, 12, 26), PAT(6)},
    {"MEMBRANA", {22, 48, 48, 64, 100, 26, 46, 48}, {0, 100, 127, 72}, 0, 0, FX(0, 12, 18, 34), PAT(3)},
    {"BORDE METAL", {104, 96, 58, 116, 112, 48, 40, 72}, {0, 100, 127, 80}, 0, 0, FX(8, 18, 22, 38), PAT(7)},
    {"PULSO HUECO", {46, 22, 104, 44, 72, 18, 64, 18}, {0, 100, 127, 68}, 0, 0, FX(0, 6, 14, 20), PAT(2)},
};

static const engine_t ENG_PARCHE = {
    .name = "PARCHE",
    .page_title = {"PIEL", "GOLPE"},
    .edit = {
        {"HARM", F_PCT, 0, 127, 64, 0, 0},
        {"BRIT", F_PCT, 0, 127, 48, 0, 0},
        {"DAMP", F_PCT, 0, 127, 72, 0, 0},
        {"EDGE", F_PCT, 0, 127, 32, 0, 0},
        {"ACC", F_PCT, 0, 127, 90, 0, 0},
        {"BEND", F_PCT, 0, 127, 12, 0, 0},
        {"BODY", F_PCT, 0, 127, 72, 0, 0},
        {"SNAP", F_PCT, 0, 127, 28, 0, 0},
    },
    .presets = PARCHE_PRESETS,
    .npresets = NELEM(PARCHE_PRESETS),
    .note_on = parche_note_on,
    .render = parche_render,
    .knob = {P_E1, P_E2, P_E3, P_E6},
    .poly = PARCHE_POLY,
    .mem = sizeof(parche_slot_t) * PARCHE_POLY,
};
