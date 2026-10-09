# MajulaRecomp — Dark Souls II (Xbox 360) no Nintendo Switch

Porte nativo de **Dark Souls II** (FromSoftware, 2014, versão de Xbox 360, title ID `465307E4`) para o Nintendo Switch
por **recompilação estática**: o código PowerPC do `Default.xex` é traduzido para C++ com o
[ReXGlue](https://github.com/rexglue/rexglue-sdk) e compilado para o Switch com o devkitA64. Não é um emulador.

> Repositório privado e em desenvolvimento. **Nenhum arquivo do jogo entra aqui**: nem a ISO, nem o código gerado a
> partir do `Default.xex`, nem os shaders traduzidos, nem binários compilados. Cada um gera tudo a partir da própria
> cópia.

## De onde vem a base

O método e quase todo o código de infraestrutura vêm dos portes irmãos:

| Projeto | O que usamos |
|---|---|
| [ChanseyIsTheBest/sa2-nx](https://github.com/ChanseyIsTheBest/sa2-nx) | **Base deste repositório** (commit `fdd6f14`, 2026-10-08): `sdk/` (ReXGlue 0.10 com a camada Horizon), `tools/`, `shaders/` (XenosRecomp), e o guia [porting-guide.md](https://github.com/ChanseyIsTheBest/sa2-nx/blob/main/docs/porting-guide.md), que é o roteiro que seguimos |
| [StevensND/nfsmw-nx](https://github.com/StevensND/nfsmw-nx) | Camada do Switch, driver NVK (Mesa) com patch, PGO/LTO, vídeo WMV3, notas de plataforma |
| [ChanseyIsTheBest/futari-nx](https://github.com/ChanseyIsTheBest/futari-nx) | Outro jogo de disco (ISO); caderno compartilhado de otimizações (`xbox360.md`) |

`sdk/` está **idêntico** ao sa2-nx de propósito, para que as otimizações futuras desses repositórios possam ser
trazidas com um diff simples. O que é específico do Dark Souls II fica em `app/` e `docs/`. Em `tools/`, `shaders/` e
no renderizador (`app/src/me`, copiado do sa2-nx) os nomes `sa2` viraram `ds2`; as ferramentas que dependem de
endereços do jogo ainda precisam dos dados do DS2.

## Estado

| Etapa (guia do sa2-nx) | Estado |
|---|---|
| 2. Arquivos do jogo extraídos e inspecionados | **feito** (ver [docs/ds2-notes.md](docs/ds2-notes.md)) |
| 3. Tradução do código (`rexglue codegen`) | **feito**: 83.143 funções, 0 erros de validação; varreduras de ponteiros e de tabelas de salto limpas |
| 4. Primeiro boot no PC com a GPU emulada | **feito**: menus, criação de save, vídeo de abertura e carregamento do primeiro mapa, sem crash do código do jogo; a GPU emulada não desenha o 3D no lavapipe (SPIR-V rejeitado) |
| 5–6. Renderizador nativo Vulkan e biblioteca de shaders | em andamento: núcleo do sa2-nx portado e rodando no lavapipe (menus, vídeo de abertura e carregamento certos, ~16 quadros/s); biblioteca com 1.383 de 1.409 shaders; ao entrar no primeiro mapa 3D há shaders sem identificar e o lavapipe cai |
| 7. Build para o Switch (devkitA64 + NVK) | pendente |
| 8–11. Medição no console e otimizações de CPU/GPU | pendente |

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

# 3. Traduzir o Default.xex para C++ (app\generated, ~350 MB; cerca de 7 min numa máquina de 4 threads)
powershell -File tools\codegen.ps1

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

O build do Switch segue
[docs/building.md do sa2-nx](https://github.com/ChanseyIsTheBest/sa2-nx/blob/main/docs/building.md), trocando `sa2`
por `ds2`; entra aqui quando for validado neste jogo.

## Licença

GPL-3.0, como os projetos de origem (ver [LICENSE](LICENSE) e [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)).
Dark Souls II é propriedade da FromSoftware / Bandai Namco; este projeto não distribui nada do jogo.
