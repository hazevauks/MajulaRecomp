# Dark Souls II (Xbox 360): notas técnicas

Fatos medidos no jogo, cada um com a ferramenta ou o log que o mostrou. O que ainda é suposição está marcado como tal.

## O disco

`Dark Souls II (World) (En,Ja,Fr,De,Es,It,Pl,Ru).iso`, 7.838.695.424 bytes, XGD2 (partição do jogo em `0x0FD90000`).
`tools/extract_iso.py --list` (2026-10-09): 121 arquivos, 19 pastas, 6.131.414.015 bytes.

| Arquivo(s) | Tamanho | O que é |
|---|---|---|
| `Default.xex` | 21,9 MB | O único executável do disco |
| `GameData1.bdt` + `.bhd`, `GameData2.bdt` + `.bhd` | 2,84 GB + 2,46 GB | Arquivos de dados do motor (BHD = índice, BDT = dados) |
| `sound/*.fsb`, `*.fev` | ~0,5 GB | Bancos do FMOD |
| `movie/prologue.wmv` | 298 MB | O único vídeo; WMV |
| `shader/*.bnd`, `shader/Simple/*.fxo`, `font/*.fxo` | ~2,8 MB | Shaders do jogo (`FlverShader.bnd` 2,47 MB, `SfxShader.bnd`, `Filter.bnd`, `Decal.bnd`, `FeShader.bnd`) |
| `font/<idioma>/FeFont_Small.fontbnd.dcx`, `regulation.bnd.dcx`, `sfx/`, `param/` | pequenos | Fontes, parâmetros, efeitos |
| `$SystemUpdate/`, `AvatarAssetPack`, `nxeart` | — | Do sistema do Xbox 360; o porte não usa |

**Shaders.** Os `.fxo` soltos (`shader/Simple/`, `font/`) são contêineres de shader do Xenos sem compressão: começam
com `102A1100` (pixel) ou `102A1101` (vértice), o formato que o XenosRecomp lê. Os `.bnd` são arquivos `BND4` com
2.203 (`FlverShader`), 241 (`SfxShader`), 105 (`Filter`), 18 (`Decal`) e 8 (`FeShader`) entradas, **2.575 no total**;
a assinatura `102A110x` não aparece neles em claro, então as entradas estão comprimidas ou embrulhadas (formato ainda
não aberto). Para comparação, a biblioteca do sa2-nx tem 35 shaders: aqui a tradução prévia e o prewarm de pipelines
têm outra escala.

Não há outro `.xex` na lista de arquivos, então o manifesto não tem `[[modules]]`. **Não verificado:** se existe
código dentro dos `GameData*.bdt` (improvável num jogo de disco, mas só o boot confirma: um `XexLoadImage` apareceria
no log).

## O executável

Cabeçalho XEX2 lido direto do arquivo (`tools/extract_iso.py --info` e leitura dos cabeçalhos opcionais, 2026-10-09):

| Campo | Valor |
|---|---|
| Title ID | `465307E4` |
| Media ID | `72EA8B94` |
| Versão | 0.0.0.8 |
| Nome original do PE | `FRPG2.exe` |
| Data do PE | 2014-01-27 04:52:15 UTC |
| Endereço base | `0x82000000` |
| Ponto de entrada | `0x82993BB0` |
| Tamanho da imagem | `0x016C0000` (23.855.104 bytes) |
| Criptografia / compressão | AES-128 "normal" / básica |
| Pilha padrão | 262.144 bytes |
| TLS | 64 slots, dados em `0x83643A00`, 1.460 bytes |
| Recurso | `465307E4` em `0x83650000`, 435.802 bytes |

Bibliotecas importadas: `xam.xex` (210 registros) e `xboxkrnl.exe` (353 registros), ambas versão 2.0.21256.0, mínimo
2.0.16537.0.

Bibliotecas estáticas (o XDK embutido no jogo), todas **2.0.21256.0**: `XNET`, `XONLINE`, `XAPILIB`, `D3D9`, `D3DX9`,
`XGRAPHC`, `XBOXKRNL`, `XAUDIO2`, `XAPOBA`, `XMP`, `XMIC`, `XMCORE`, `XHV2`, `XPARTY`, `XWMAD`. Compilador: `C1`/`C2`
16.0.11886.0, `LINK` 10.0.11886.0, `LIBCPMT` 2.0.11886.0.

O que isso já diz para o porte:

- **Direct3D do XDK 21256** (setembro de 2013: os caminhos de build na imagem dizem `e:\xenon\xdksep13\`). O layout
  da estrutura do device (guia, seção 10) depende dessa versão; o sa2-nx documenta o layout "2012" e as diferenças do
  Futari. Nenhuma menção ao build 21256 nos repositórios de referência: os offsets terão de ser conferidos neste jogo.
- **Rede e voz** (`XNET`, `XONLINE`, `XHV2`, `XPARTY`, `XMIC`): o jogo espera o Xbox Live para o multijogador. O porte
  roda offline; ver como o XAM do SDK responde às chamadas de sessão é trabalho do primeiro boot.

## Middleware (strings da imagem descomprimida)

Lidas de `out/default_image.bin` (escrita pelo `rexglue codegen` com `REXGLUE_DUMP_IMAGE`), 2026-10-09:

| Biblioteca | Evidência |
|---|---|
| FMOD Ex 4.40.05, com o codec XMA do Xbox 360 | `N:\FMOD_FRPG2\Source\fmodexsrc44005\fmod4\xbox360\src\fmod_codec_xma.cpp`, `FMOD XMA Codec`, 188 strings |
| Havok 2012.1.0-r1 (física e animação) | `hk_2012.1.0-r1`, `Havok Job Thread`, `HavokAnimationSampler` |
| Decodificador WMV do XDK, em software | `...\xdksep13\...\codecs\wmv\codecdsp-cayenne\video\wmv\decoder_c9e\...` |
| zlib 1.2.7 | `deflate 1.2.7 Copyright 1995-2012` |

- O áudio sai pelo XAudio2 com XMA por baixo do FMOD: é o caminho que o SDK já cobre (decodificador XMA + mixer).
- O vídeo (`movie/prologue.wmv`) é decodificado pelo próprio código do jogo, que será recompilado junto. No Switch
  isso pode pesar; o nfsmw-nx tem um decodificador WMV3 nativo (`nfsmw_video_wmv3.cpp`) como alternativa.
- A única string `.xex` da imagem é `xam.xex`: nada indica módulos carregados com `XexLoadImage`.
- 108 nomes de shader terminados em `.fpo` (pixel) / `.vpo` (vértice) aparecem na imagem (por exemplo
  `BilateralBlurAO4.fpo`, `BlackOut_Vs.vpo`).

## A máquina de build

Medido em 2026-10-09: 4 threads lógicas, 12 GB de RAM, 232 GB livres. O link LTO do sa2-nx (imagem de ~39 MB) já
pede 15–30 min e bastante memória numa máquina maior; aqui o `-flto` terá de usar poucos jobs e talvez ficar
desligado nos builds do dia a dia.

devkitPro em `C:\Users\Usuario\Downloads\devkitPro` (devkitA64 GCC 16.1.0, libnx, `portlibs/switch`). A variável
`DEVKITPRO` do sistema vale `/opt/devkitpro` (caminho do MSYS2), e o registro do Windows aponta para um caminho antigo
(`Downloads\trabalho\devkitPro`): os scripts de build do Switch precisam do caminho real.
