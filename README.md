## MajulaRecomp — Dark Souls II on Nintendo Switch

![Banner](Banner.jpg)


**Dark Souls II (2014)** from Xbox 360 statically recompiled for the handheld console.  
A native port, unlike certain "ports" that are disguised emulators.



## De onde vem a base

O método e quase todo o código de infraestrutura vêm dos portes irmãos:

| Projeto | O que usamos |
|---|---|
| [ChanseyIsTheBest/sa2-nx](https://github.com/ChanseyIsTheBest/sa2-nx) | **Base deste repositório** (commit `fdd6f14`, 2026-10-08): `sdk/` (ReXGlue 0.10 com a camada Horizon), `tools/`, `shaders/` (XenosRecomp), e o guia [porting-guide.md](https://github.com/ChanseyIsTheBest/sa2-nx/blob/main/docs/porting-guide.md), que é o roteiro que seguimos |
| [StevensND/nfsmw-nx](https://github.com/StevensND/nfsmw-nx) | Camada do Switch, driver NVK (Mesa) com patch, PGO/LTO, vídeo WMV3, notas de plataforma |
| [ChanseyIsTheBest/futari-nx](https://github.com/ChanseyIsTheBest/futari-nx) | Outro jogo de disco (ISO); caderno compartilhado de otimizações (`xbox360.md`) |

`sdk/` está **idêntico** ao sa2-nx de propósito (quatro exceções, marcadas `ds2-nx`: a gravação do áudio com o som mudo, em `sdk/src/audio/sdl/sdl_audio_driver.cpp`; o teclado do sistema do Switch para `XamShowKeyboardUI`, em `sdk/src/kernel/xam/xam_ui.cpp`; a leitura dos clocks sem manter sessões `clkrst` abertas, em `sdk/src/ui/switch_perf.cpp`; e a pilha de host das threads do jogo proporcional à pilha de guest, em vez de 16 MB fixos, em `sdk/src/system/xthread.cpp`), para que as otimizações futuras desses repositórios possam ser
trazidas com um diff simples. O que é específico do Dark Souls II fica em `app/` e `docs/`. Em `tools/`, `shaders/` e
no renderizador (`app/src/me`, copiado do sa2-nx) os nomes `sa2` viraram `ds2`; as ferramentas que dependem de
endereços do jogo ainda precisam dos dados do DS2.

## Estado

| Etapa (guia do sa2-nx) | Estado |
|---|---|
| 2. Arquivos do jogo extraídos e inspecionados | **feito** (ver [docs/ds2-notes.md](docs/ds2-notes.md)) |
| 3. Tradução do código (`rexglue codegen`) | **feito**: 83.143 funções, 0 erros de validação; varreduras de ponteiros e de tabelas de salto limpas |
| 4. Primeiro boot no PC com a GPU emulada | **feito**: menus, criação de save, vídeo de abertura e carregamento do primeiro mapa, sem crash do código do jogo; a GPU emulada não desenha o 3D no lavapipe (SPIR-V rejeitado) |
| 5–6. Renderizador nativo Vulkan e biblioteca de shaders | em andamento: núcleo do sa2-nx portado e rodando no lavapipe; biblioteca com os 1.409 shaders do disco; **o jogo aparece certo no primeiro mapa 3D** (cena, HUD, tone mapping), a ~0,5 quadro/s por software, sem nenhum desenho rejeitado ou sem shader em 25 min na área inicial (morte e recarga incluídas); falta percorrer o resto do jogo, o que este PC sem GPU torna muito lento (ver as notas) |
| 7. Build para o Switch (devkitA64 + NVK) | **roda no console** (codegen clássico, sem LTO/PGO; driver NVK do nfsmw-nx compilado aqui): menus, criação de personagem com o teclado do sistema, save, áudio e o primeiro mapa sem desenhos rejeitados ([docs/switch-testing.md](docs/switch-testing.md)) |
| 8–11. Medição no console e otimizações de CPU/GPU | **em andamento**: na primeira fogueira, modo portátil a 720p sem overclock, de 7,3 para 12,7 quadros/s (GPU de 137,9 para 78,8 ms por quadro) em duas rodadas; 540p no portátil com imagem e sombras confirmadas no console, jogado do início até a Forest of Fallen Giants: 19 quadros/s parado na primeira fogueira, 12–16 na área do tutorial; o limite passou a ser a CPU (2,9 dos 3 núcleos ocupados; dois terços são o código do jogo, um quinto o renderizador, 4 % o driver Vulkan). Menus a 43–60. Falta CPU (perfil de pilhas), LTO/PGO e o modo TV |

## Estrutura

```text
app/      a aplicação do DS2: manifesto do codegen, correções de análise (overrides.toml), fontes próprias
sdk/      ReXGlue SDK (fork do sa2-nx): runtime do Xbox 360 em C++, gerador de código, camada Horizon
shaders/  XenosRecomp e as ferramentas da biblioteca de shaders
tools/    extração, análise do código gerado, scripts de build (tools/win) e de medição
mesa/     notas do driver NVK
docs/     notas técnicas do Dark Souls II
```

Fora do git (ver `.gitignore`): `assets/` (jogo extraído), `app/generated/` (C++ gerado), `out/` (builds), `_ref/`
(clones dos repositórios de referência) e `_tools/` (ferramentas portáteis, como o LLVM).

## Como reproduzir o que já existe

Pré-requisitos no Windows: VS 2022 Build Tools (workload C++; traz CMake e Ninja), LLVM clang 20 em `_tools\llvm`
(ou `DS2_LLVM`), Python 3.12 e Git. Tudo roda do PowerShell, nunca do shell do MSYS2 do devkitPro.

```powershell
# 1. Extrair a ISO (só leitura) para assets\game_root e mostrar o cabeçalho do XEX
python tools\extract_iso.py "<caminho>\Dark Souls II.iso"

# 2. Fontes de terceiros do SDK e o gerador de código
python tools\fetch_thirdparty.py
powershell -File tools\win\run.ps1 out\host_configure.log "cmake -S sdk -B out/host -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_FLAGS=-march=x86-64-v2 -DCMAKE_CXX_FLAGS=-march=x86-64-v2"
powershell -File tools\win\run.ps1 out\host_build.log "cmake --build out/host --target rexglue"

# 3. Traduzir o Default.xex para C++ (app\generated, ~350 MB; cerca de 13 min numa máquina de 4 threads).
#    -Flavor optimised é o código entregue (app\perf_overrides.toml); sem ele sai o código "clássico".
powershell -File tools\codegen.ps1 -Flavor optimised

# 4. Conferências depois de cada codegen (precisam de out\default_image.bin: rode o passo 3 uma vez com
#    $env:REXGLUE_DUMP_IMAGE = "$PWD\out\default_image.bin")
python tools\switch_check.py
python tools\pointer_audit.py

# 5. Build de PC (39 min na primeira vez em 4 threads; depois só o que mudou) e teste de boot
powershell -File tools/win/run.ps1 out/pc_configure.log "cmake --preset win-pc -S app"
powershell -File tools/win/run.ps1 out/pc_build.log "cmake --build out/pc -- -j4"
powershell -File tools/win/boot_test.ps1 -VideoAt 3 -VideoFrames 50 -VideoEvery 60
```

O teste de boot roda o jogo mudo e minimizado, grava um quadro por segundo e gera
`out/pc/captures/video_3s_sheet.png`. Num PC cuja GPU não tem Direct3D 12 (o de desenvolvimento, com Intel HD 4000),
defina antes `$env:REX_D3D12_ADAPTER = '-2'` para usar o rasterizador por software da Microsoft (WARP).

### Renderizador nativo no PC (Vulkan por software)

```powershell
# glslangValidator (uma vez) e a biblioteca de shaders, feita dos arquivos do disco (~35 min em 4 threads)
powershell -File tools/win/build_glslang.ps1
python tools/extract_shaders.py                        # out/shaders_game: 1.409 contêineres
bash tools/build_shader_library.sh out/shaders_game    # out/shaders/ds2_shaders.mesp + .idx
copy out/shaders/ds2_shaders.mesp* out/pc/

# Rodar com o renderizador nativo sobre o lavapipe (Mesa em _tools/mesa; ver docs/ds2-notes.md)
$icd = (Resolve-Path _tools/mesa/x64/lvp_icd.x86_64.json).Path
$env:VK_DRIVER_FILES = $icd; $env:VK_ICD_FILENAMES = $icd
$env:REX_DS2_RENDERER_NATIVE = 'true'
powershell -File tools/win/boot_test.ps1 -VideoAt 3 -VideoFrames 60 -VideoEvery 60 -Pad '16-16.6:a,30-30.6:start'
```

A biblioteca usada nos testes também leva os contêineres que o renderizador reconstrói das cargas imediatas do
Direct3D (`$env:REX_DS2_SHADER_MISSES = 'shader_misses'` grava em `out/pc/shader_misses/missing`; passe essa pasta
como segundo argumento do `build_shader_library.sh`). Ferramentas pedidas: DXC em `_tools/dxc` e o clang de
`_tools/llvm`. Se o processo cair, `python tools/minidump_summary.py` diz em que módulo foi.

O build do Switch segue
[docs/building.md do sa2-nx](https://github.com/ChanseyIsTheBest/sa2-nx/blob/main/docs/building.md), trocando `sa2`
por `ds2`; entra aqui quando for validado neste jogo.

## Licença

GPL-3.0, como os projetos de origem (ver [LICENSE](LICENSE) e [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)).
Dark Souls II é propriedade da FromSoftware / Bandai Namco; este projeto não distribui nada do jogo.
