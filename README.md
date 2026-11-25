# Acid Lead (VST3) — synth de lead ácido para psytrance

Monofônico: 2 osciladores PolyBLEP (saw/square) + sub, envelope de filtro com decay, accent por velocity (>= ~100),
slide em notas sobrepostas (legato), drive.

Filtro selecionável:
- **MS-20** (padrão): passa-baixa 12 dB na topologia Korg 35 (Sallen-Key com realimentação por high-pass),
  TPT/ZDF, clipper (tanh) na realimentação + HPF de 12 dB antes do filtro, como no MS-20.
- **Ladder (303)**: ladder 24 dB não linear.

Controle **Analog**: drift lento e independente de pitch (até ~8 cents) e cutoff, simulando a instabilidade dos VCOs/VCF.

## Compilar
Requisitos: CMake 3.22+, Git, e um compilador (Visual Studio 2022 / Xcode / GCC-Clang + libs de dev do JUCE no Linux).

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

O JUCE é baixado automaticamente. O .vst3 é copiado para a pasta de plugins ao final do build
(use o alvo Standalone para testar sem DAW).

## Tocar
- Nota normal: filtro reabre a cada nota.
- Velocity alto (accent): mais abertura, ressonância, volume e decay mais curto.
- Notas sobrepostas (legato): slide, sem retrigger do envelope.
- Linha típica psy lead: 1/16, cutoff 300-1200 Hz, reso 0.7-0.9, env mod 0.5-0.8, decay 150-300 ms.

## Sem instalar nada: build automático no GitHub
1. Crie um repositório no GitHub e suba o conteúdo desta pasta (incluindo `.github/`).
2. Aba **Actions** -> workflow **Build VST3** (roda sozinho a cada push, ou clique em *Run workflow*).
3. Ao terminar (~10-15 min), baixe o artefato **AcidLead-Windows** / **-macOS** / **-Linux**:
   dentro de `VST3/` está o **Acid Lead.vst3**.
4. Copie para a pasta de plugins: Windows `C:\Program Files\Common Files\VST3`,
   macOS `~/Library/Audio/Plug-Ins/VST3`, Linux `~/.vst3`.
   No macOS, se o DAW bloquear: `xattr -cr "Acid Lead.vst3"`.
