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

`sdk/` e `shaders/` estão **idênticos** ao sa2-nx de propósito, para que as otimizações futuras desses repositórios
possam ser trazidas com um diff simples. O que é específico do Dark Souls II fica em `app/` e `docs/`. Em `tools/`, os
nomes `sa2` viraram `ds2`; as ferramentas que dependem de endereços do jogo ainda precisam dos dados do DS2.

## Estado

| Etapa (guia do sa2-nx) | Estado |
|---|---|
| 2. Arquivos do jogo extraídos e inspecionados | **feito** (ver [docs/ds2-notes.md](docs/ds2-notes.md)) |
| 3. Tradução do código (`rexglue codegen`) | pendente: falta o toolchain de PC (clang 20, VS Build Tools, CMake, Ninja, Python) |
| 4. Primeiro boot no PC com a GPU emulada | pendente |
| 5–6. Renderizador nativo Vulkan e biblioteca de shaders | pendente |
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

```powershell
# 1. Extrair a ISO (só leitura) para assets\game_root e mostrar o cabeçalho do XEX
python tools\extract_iso.py "<caminho>\Dark Souls II.iso"
```

Os passos seguintes (gerador de código, build de PC, build do Switch) são os de
[docs/building.md do sa2-nx](https://github.com/ChanseyIsTheBest/sa2-nx/blob/main/docs/building.md), trocando `sa2`
por `ds2`; serão documentados aqui conforme forem validados neste jogo.

## Licença

GPL-3.0, como os projetos de origem (ver [LICENSE](LICENSE) e [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)).
Dark Souls II é propriedade da FromSoftware / Bandai Namco; este projeto não distribui nada do jogo.
