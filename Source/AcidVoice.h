#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// Parâmetros lidos uma vez por bloco de áudio.
struct AcidParams
{
    float wave        = 0.0f;    // 0 = saw, 1 = square
    float detuneCents = 8.0f;    // detune do 2º oscilador
    float subLevel    = 0.0f;    // sub-oscilador (seno, 1 oitava abaixo)
    float cutoffHz    = 800.0f;
    float reso        = 0.7f;    // 0..1 (1 = limite da auto-oscilação)
    float envMod      = 0.6f;    // quanto o envelope abre o filtro
    float decayMs     = 250.0f;  // decay do envelope do filtro
    float accent      = 0.7f;    // intensidade do accent
    float glideMs     = 60.0f;   // portamento (apenas em notas ligadas / slide)
    float drive       = 0.3f;    // saturação pré-filtro
    float releaseMs   = 40.0f;   // release do amp
    int   filterType  = 1;       // 0 = ladder (303), 1 = estilo Korg MS-20
    float hpfHz       = 20.0f;   // high-pass 12 dB antes do filtro (como no MS-20)
    float analog      = 0.3f;    // drift de pitch/cutoff (instabilidade analógica)
};

// Filtro ladder 24 dB/oct não linear (estilo Moog/TB-303), 2x oversampling interno.
class LadderFilter
{
public:
    void reset() { for (int i = 0; i < 4; ++i) { y[i] = 0.0; t[i] = 0.0; } }

    void setParams (double cutoffHz, double resonance, double sampleRate)
    {
        const double fs2 = 2.0 * sampleRate;
        const double fc  = std::min (cutoffHz, 0.45 * sampleRate);
        g = 1.0 - std::exp (-6.283185307179586 * fc / fs2);
        k = 4.0 * resonance;
    }

    float process (float in)
    {
        for (int i = 0; i < 2; ++i)
        {
            const double u = std::tanh ((double) in - k * y[3]);
            y[0] += g * (u    - t[0]); t[0] = std::tanh (y[0]);
            y[1] += g * (t[0] - t[1]); t[1] = std::tanh (y[1]);
            y[2] += g * (t[1] - t[2]); t[2] = std::tanh (y[2]);
            y[3] += g * (t[2] - t[3]); t[3] = std::tanh (y[3]);
        }
        return (float) y[3];
    }

private:
    double y[4] {}, t[4] {};
    double g = 0.1, k = 0.0;
};

// Filtro passa-baixa 12 dB/oct na topologia "Korg 35" (Sallen-Key com realimentação
// via high-pass), a mesma família do filtro do MS-20. Implementado em TPT/ZDF, com o
// laço sem atraso resolvido analiticamente. A realimentação passa por um clipper
// (tanh), que dá o "grito" característico quando a ressonância sobe.
class Ms20Filter
{
public:
    void reset() { s1 = s2 = sh = 0.0; }

    void setParams (double cutoffHz, double resonance, double sampleRate)
    {
        const double fc = std::min (cutoffHz, 0.45 * sampleRate);
        const double g  = std::tan (3.141592653589793 * fc / sampleRate);
        G     = g / (1.0 + g);
        K     = 1.995 * std::clamp (resonance, 0.0, 1.0);   // K = 2 => auto-oscilação
        alpha = 1.0 / (1.0 - K * G + K * G * G);
    }

    float process (float x)
    {
        // estágio 1: LP de um polo
        const double y1 = G * x + (1.0 - G) * s1;
        s1 = 2.0 * y1 - s1;

        // saída do estágio 2 (solução linear do laço) -> usada só para o sinal de realimentação
        const double y2lin = alpha * (G * y1 + (1.0 - G) * (s2 - G * sh));
        const double f     = std::tanh (K * y2lin);          // clipper na realimentação

        // high-pass de um polo sobre o sinal de realimentação
        const double lph = G * f + (1.0 - G) * sh;
        const double hp  = f - lph;
        sh = 2.0 * lph - sh;

        // estágio 2: LP de um polo
        const double u  = y1 + hp;
        const double y2 = G * u + (1.0 - G) * s2;
        s2 = 2.0 * y2 - s2;

        return (float) y2;
    }

private:
    double s1 = 0.0, s2 = 0.0, sh = 0.0;
    double G = 0.1, K = 0.0, alpha = 1.0;
};

// High-pass de 12 dB/oct (dois polos TPT em cascata, sem ressonância).
class HighPass12
{
public:
    void reset() { s[0] = s[1] = 0.0; }

    void setCutoff (double fc, double sampleRate)
    {
        const double g = std::tan (3.141592653589793 * std::min (fc, 0.45 * sampleRate) / sampleRate);
        G = g / (1.0 + g);
    }

    float process (float x)
    {
        double v = x;
        for (int i = 0; i < 2; ++i)
        {
            const double lp = G * v + (1.0 - G) * s[i];
            s[i] = 2.0 * lp - s[i];
            v -= lp;
        }
        return (float) v;
    }

private:
    double s[2] {};
    double G = 0.01;
};

// Voz monofônica "acid": last-note priority, slide em notas sobrepostas, accent por velocity.
class AcidVoice
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        notes.reserve (64);
        reset();
    }

    void reset()
    {
        notes.clear();
        gate = false;
        ampEnv = filterEnv = 0.0;
        accentLevel = 0.0f;
        accentTarget = 0.0f;
        phase1 = phase2 = subPhase = 0.0;
        drift1 = drift2 = driftC = 0.0f;
        tgt1 = tgt2 = tgtC = 0.0f;
        driftCounter = 0;
        ladder.reset();
        ms20.reset();
        hpf.reset();
    }

    void noteOn (int note, float velocity)
    {
        const bool legato = gate && ! notes.empty();
        notes.erase (std::remove (notes.begin(), notes.end(), note), notes.end());
        notes.push_back (note);

        targetFreq   = noteToHz (note);
        accentTarget = velocity >= 0.78f ? 1.0f : 0.0f;   // velocity >= ~100 => accent

        if (! legato)
        {
            currentFreq = targetFreq;   // sem glide em nota nova
            filterEnv   = 1.0;          // retrigger do envelope do filtro
            gate        = true;
        }
        // legato: mantém o envelope e desliza (slide) até a nova nota
    }

    void noteOff (int note)
    {
        notes.erase (std::remove (notes.begin(), notes.end(), note), notes.end());

        if (notes.empty())
            gate = false;
        else
            targetFreq = noteToHz (notes.back());   // volta para a nota anterior, deslizando
    }

    void allNotesOff() { notes.clear(); gate = false; }

    // Escreve (sobrescreve) numSamples amostras mono em out.
    void render (float* out, int numSamples, const AcidParams& p)
    {
        int done = 0;

        while (done < numSamples)
        {
            const int n = std::min (kChunk, numSamples - done);

            // ---- drift analógico: random walk lento, independente por oscilador ----
            driftCounter -= n;
            if (driftCounter <= 0)
            {
                tgt1 = nextRandom(); tgt2 = nextRandom(); tgtC = nextRandom();
                driftCounter = (int) (0.25 * sr) + (int) ((nextRandom() + 1.0f) * 0.25 * sr);
            }
            const float driftCoef = (float) (1.0 - std::exp (-(double) n / (0.4 * sr)));
            drift1 += (tgt1 - drift1) * driftCoef;
            drift2 += (tgt2 - drift2) * driftCoef;
            driftC += (tgtC - driftC) * driftCoef;

            const double pitchMul1 = std::exp2 (drift1 * p.analog * 8.0 / 1200.0);   // até ~8 cents
            const double pitchMul2 = std::exp2 (drift2 * p.analog * 8.0 / 1200.0);
            const double cutMul    = std::exp2 (driftC * p.analog * 0.2);            // até ~0.2 oitava

            // ---- taxa de controle (uma vez por chunk) ----
            const float decayMs = std::max (1.0f, p.decayMs * (1.0f - 0.5f * p.accent * accentLevel));
            const double decayCoef  = std::exp (-1.0 / (decayMs * 0.001 * sr));
            const double envAmt     = p.envMod * (1.0 + 0.8 * p.accent * accentLevel);
            const double reso       = std::min (0.98, (double) p.reso + 0.15 * p.accent * accentLevel);
            const double glideCoef  = p.glideMs < 1.0f ? 0.0 : std::exp (-1.0 / (p.glideMs * 0.001 * sr));
            const double atkCoef    = 1.0 - std::exp (-1.0 / (0.002 * sr));
            const double relCoef    = 1.0 - std::exp (-1.0 / (std::max (1.0f, p.releaseMs) * 0.001 * sr));
            const float  accentCoef = (float) (1.0 - std::exp (-1.0 / (0.005 * sr)));
            const double detRatio   = std::exp2 (p.detuneCents / 1200.0);
            const float  level      = 0.7f * (1.0f + 0.5f * p.accent * accentLevel);
            const float  preGain    = 1.0f + p.drive * 4.0f;
            const bool   useMs20    = p.filterType == 1;

            const double cutoff = std::clamp (p.cutoffHz * cutMul * std::exp2 (envAmt * filterEnv * 4.5),
                                              20.0, 0.45 * sr);
            if (useMs20) ms20.setParams (cutoff, reso, sr);
            else         ladder.setParams (cutoff, reso, sr);
            hpf.setCutoff (p.hpfHz, sr);

            // ---- taxa de áudio ----
            for (int i = 0; i < n; ++i)
            {
                currentFreq = targetFreq + (currentFreq - targetFreq) * glideCoef;
                accentLevel += (accentTarget - accentLevel) * accentCoef;
                filterEnv   *= decayCoef;
                ampEnv      += ((gate ? 1.0 : 0.0) - ampEnv) * (gate ? atkCoef : relCoef);

                const double dt1 = std::min (0.45, currentFreq * pitchMul1 / sr);
                const double dt2 = std::min (0.45, currentFreq * pitchMul2 * detRatio / sr);

                const float o1 = oscSample (phase1, dt1, p.wave);
                const float o2 = oscSample (phase2, dt2, p.wave);

                subPhase += dt1 * 0.5;
                if (subPhase >= 1.0) subPhase -= 1.0;
                const float sub = (float) std::sin (6.283185307179586 * subPhase);

                const float mix = 0.5f * (o1 + o2) + p.subLevel * sub;
                const float pre = hpf.process (std::tanh (mix * preGain));

                const float filtered = useMs20 ? ms20.process (pre) * kMs20Gain
                                               : ladder.process (pre);

                out[done + i] = filtered * (float) ampEnv * level;
            }

            done += n;
        }
    }

private:
    static constexpr int kChunk = 16;
    static constexpr float kMs20Gain = 0.27f;   // compensação de nível entre filtros

    static double noteToHz (int note) { return 440.0 * std::exp2 ((note - 69) / 12.0); }

    float nextRandom()   // xorshift32 -> [-1, 1)
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) (rng >> 8) / 8388608.0f - 1.0f;
    }

    static double polyBlep (double t, double dt)
    {
        if (t < dt)       { t /= dt;           return t + t - t * t - 1.0; }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
        return 0.0;
    }

    static float oscSample (double& phase, double dt, float wave)
    {
        const double saw = 2.0 * phase - 1.0 - polyBlep (phase, dt);
        double sq = (phase < 0.5 ? 1.0 : -1.0) + polyBlep (phase, dt);
        sq -= polyBlep (std::fmod (phase + 0.5, 1.0), dt);

        phase += dt;
        if (phase >= 1.0) phase -= 1.0;

        return (float) (saw + wave * (sq - saw));
    }

    double sr = 44100.0;
    std::vector<int> notes;
    bool gate = false;

    double targetFreq = 110.0, currentFreq = 110.0;
    double ampEnv = 0.0, filterEnv = 0.0;
    float accentLevel = 0.0f, accentTarget = 0.0f;
    double phase1 = 0.0, phase2 = 0.0, subPhase = 0.0;

    float drift1 = 0.0f, drift2 = 0.0f, driftC = 0.0f;
    float tgt1 = 0.0f, tgt2 = 0.0f, tgtC = 0.0f;
    int driftCounter = 0;
    uint32_t rng = 0x1234abcd;

    LadderFilter ladder;
    Ms20Filter   ms20;
    HighPass12   hpf;
};
