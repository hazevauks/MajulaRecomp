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

## Primeiro boot no PC (2026-10-09)

Build `win-pc` (clang 20, RelWithDebInfo, codegen clássico), GPU emulada do SDK em Direct3D 12 sobre o WARP
(`REX_D3D12_ADAPTER=-2`), 60 s de execução com `tools/win/boot_test.ps1 -VideoAt 3 -VideoFrames 50 -VideoEvery 60`
(log `out/pc/logs/ds2_003.log`, quadros em `out/pc/captures/video_3s.rgba`):

- O processo fica de pé os 60 s, sem "unregistered function", sem exceção do host e sem `REX_FATAL`.
- Quadros 0–8 (3 a 11 s): tela preta com o ícone de carregamento (fogueira + "DARK SOULS II") no canto inferior
  esquerdo. Do quadro 9 em diante: a caixa "This game uses an autosave function. Do not turn off the console or
  remove the storage device while the save indicator is shown at the top of the screen." com o botão OK. Texto,
  texturas e transparência corretos a 640x360 (metade de 1280x720).
- O jogo fica parado nessa caixa: o teste não envia entrada. **Nada além disso foi visto ainda** (menu, criação de
  personagem, jogo, vídeo de abertura).
- Áudio: o FMOD abre o dispositivo e o mixer entrega ~1.877 quadros a cada 10 s sem silêncio por falta de dados (rodando
  mudo; o som em si não foi ouvido).
- Arquivos que o jogo procura e não acha, sem consequência visível até aqui: `game:\config.properties`,
  `game:\userconfig.properties`, `game:\param\KeyConfigParam.param`, e os dispositivos `cache:\` e `install:\`
  (não montados pelo SDK).
- Aviso repetido da GPU emulada durante o carregamento: `Texture fetch constant (88000000 13FC8852 ...) has "invalid"
  type`, cerca de 4 por segundo durante 9 s; para quando a caixa aparece.

**GPU do PC de desenvolvimento.** Intel HD Graphics 4000 (driver 10.18.10.4252, de 2015): sem Direct3D 12 e sem driver
Vulkan. A GPU emulada só abre no WARP, por software, o que basta para conferir imagem mas não serve de medida de
velocidade. O renderizador nativo (Vulkan) não tem como rodar neste PC com a GPU real: as saídas são um Vulkan por
software (lavapipe) no PC, outro PC, ou testar direto no Switch.

## Até onde o jogo vai no PC (2026-10-09)

Com a entrada por script (`ds2_input_script`, `tools/win/boot_test.ps1 -Pad`) e a GPU emulada, sem nenhum crash,
"unregistered function" ou `REX_FATAL` em sessões de até 15 minutos:

1. Aviso de autosave → logos (Bandai Namco, FromSoftware, Havok) → tela-título com "PRESS START BUTTON".
2. Start → "Checking save data" → "Saving content" (o save é criado em `Documents\ds2\<perfil>\465307E4\`) → aviso
   "Ethernet cable is not connected" com OK.
3. Menu principal (New Game / Continue / Go Offline) → New Game → ajuste de brilho → opções → "Saving content".
4. Numa segunda sessão, Continue → lista de dados → vídeo de abertura (`movie/prologue.wmv`: começa preto, só com
   narração, legendas e créditos; depois a floresta em CGI, com as barras de cinema) → tela de carregamento com
   descrição de item (correta).

O vídeo de abertura é decodificado pelo código do próprio jogo e desenhado com `DrawVertices` (tipo 4, 6 vértices por
quadro). **No backend Vulkan (lavapipe) ele sai certo** (`out/pc/logs/ds2_008.log`, quadros 50–79 de
`video_100s.rgba`), a ~4 trocas de quadro por segundo. No Direct3D 12 sobre o WARP o mesmo vídeo saía como um
retângulo azul liso: defeito daquele caminho, não investigado, e sem importância para o porte (o Switch usa Vulkan).

**O que ainda não foi visto ou verificado:**

- O jogo em 3D de verdade (Things Betwixt): as sessões acabaram durante o vídeo de abertura. Nenhuma cena 3D do motor
  foi desenhada ainda.
- Com 3–8 quadros/s nos menus, um toque de 150 ms do script se perde entre duas leituras do controle: usar faixas
  (`135-135.6:a`).
- Som: o FMOD decodifica XMA o tempo todo (cerca de 1.300 contextos a cada 10 s nos menus), mas tudo roda mudo.

## Direct3D do jogo (rastreio de 2026-10-09)

Candidatos tirados dos cabeçalhos PM4 que cada função monta (`lis rX,0xC0nn; ori`) e conferidos em execução com
`ds2_d3d_trace = true` (`app/src/ds2_d3d_trace.cpp`; log `out/pc/logs/ds2_007.log`). Device em `0x40042980`.

| Função | O que é | Evidência |
|---|---|---|
| `sub_829BF328` | carga de shaders na hora do desenho | escreve `IM_LOAD`; lê o VS em `device+13072` e o PS em `device+13068` (os mesmos offsets do sa2-nx, XDK "2012"); chamada de dentro de `sub_829C2340` (`0x829C2410`), 4 por quadro nos menus |
| `sub_829C2340` | `BeginVertices` (dev, tipo, contagem, stride) | escreve `DRAW_INDX` de 2 palavras; devolve o ponteiro onde `sub_829C2800` copia os vértices |
| `sub_829C2800` | `DrawVerticesUP` (dev, tipo, contagem, dados, stride) | 0x48 bytes: chama `sub_829C2340`, depois `memcpy(ptr, dados, contagem*stride)`; é o desenho dos menus (32 por quadro, tipo 6, 4 vértices de 24 bytes) |
| `sub_829C2848`, `sub_829C2D88` | `BeginIndexedVertices` / `DrawIndexedVerticesUP` (suposição pela forma) | `DRAW_INDX` de 4 palavras; não chamadas nos menus |
| `sub_829C2E28` | `DrawVertices` (dev, tipo, início, contagem) | `DRAW_INDX` de 2 palavras; chamada pelo jogo em `0x830802D0` com tipo 4, início 0, 6 vértices: o retângulo do vídeo de abertura, uma vez por quadro |
| `sub_829C3240` | `DrawIndexedVertices` (dev, tipo, base, início, contagem) | `DRAW_INDX` de 4 palavras, 0x418 bytes depois de `DrawVertices` como no sa2-nx; chamada pelo jogo em `0x82F0E500` (tipo 6, contagens de 0x66 a 0xA7F índices) |
| `sub_829BB060` | troca de quadro (Swap) | chamada pelo jogo em `0x82FECE38`, uma vez por quadro (42 em 5 s); dois `WAIT_REG_MEM` |
| `sub_829CBAE8` | desenho de retângulo interno (clear ou resolve) | `DRAW_INDX_2`, uma vez por quadro, de `0x829CBFBC` |
| `sub_829A86A8` | envio de buffer de comandos | `INDIRECT_BUFFER`, ~10 por quadro, de `0x829A92A8` |
| `sub_829A7F70` | tratador de interrupção da GPU | registrado pelo jogo em `SetInterruptCallback` |

`CreateVertexShader` / `CreatePixelShader` ainda não foram localizadas; o renderizador nativo lê os contêineres direto
dos objetos de shader na carga, então elas só fazem falta para o despejo de shaders.

## Vulkan por software no PC (lavapipe)

O Mesa 26.2.4 para Windows (`mesa3d-26.2.4-release-msvc.7z` de pal1000/mesa-dist-win, SHA-256 `351fc8c8...`) fica
extraído em `_tools\mesa`, sem instalação no sistema. Para usar:

```powershell
$icd = (Resolve-Path _tools\mesa\x64\lvp_icd.x86_64.json).Path
$env:VK_DRIVER_FILES = $icd; $env:VK_ICD_FILENAMES = $icd
$env:REX_DS2_GPU_BACKEND = 'vulkan'     # a GPU emulada em Vulkan em vez de Direct3D 12
```

Medido nos menus (log `ds2_007.log`): dispositivo `llvmpipe (LLVM 23.1.2, 256 bits)`, Vulkan 1.4; mesmas imagens do
Direct3D 12/WARP até a tela-título, a ~8,4 quadros/s (42 trocas em 5 s) contra 4–5 no WARP. É o mesmo caminho de
código (GPU emulada em Vulkan) que o Switch usa nos primeiros quadros.

## Shaders do jogo

`tools/extract_shaders.py` (2026-10-09) lê os contêineres direto do disco extraído: cada entrada dos `shader/*.bnd`
é um fluxo zlib com um contêiner (`102A1100` pixel, `102A1101` vértice), e os `.fxo` são contêineres soltos. De 2.584
entradas saem **120 shaders de vértice e 1.289 de pixel distintos** (1.174 são cópias idênticas de outros; a entrada 0
do `FlverShader.bnd` não é um contêiner). Ficam em `out/shaders_in/` com os nomes que `tools/build_shader_library.sh`
espera, então a biblioteca pode ser montada sem rodar o jogo. Faltam os shaders internos do Direct3D (clears,
cópias), que só aparecem com o despejo de descoberta do renderizador nativo.

## Primeiro mapa 3D e o limite da GPU emulada no lavapipe (2026-10-09)

Sessão `out/pc/logs/ds2_009.log` (GPU emulada em Vulkan, lavapipe): Continue, vídeo pulado com Start aos 135 s, cerca
de 150 s de tela de carregamento, e o processo cai com `0xC0000005` aos 305 s.

- Logo antes da queda o log mostra a GPU emulada criando dezenas de pipelines com shaders novos (por exemplo
  `VS D24FF649F7459F81, PS A3249875A446A537`): o jogo terminou de carregar o mapa e começou a desenhar em 3D.
- Vários desses pipelines falham: `SPIR-V parsing FAILED: Source and destination types of SpvOpStore do not match:
  int (%7) vs. float (%15)` e `vkCreateGraphicsPipelines failed (result=-13)`. É o SPIR-V que o tradutor de shaders
  da GPU emulada gera, recusado pelo analisador do Mesa.
- A violação de acesso vem em seguida. O despejo do Windows (`%LOCALAPPDATA%/CrashDumps/ds2.exe.14192.dmp`) põe o
  endereço em `vulkan_lvp.dll+0xBFDF2` (uma escrita), dentro do lavapipe.

Consequência: a GPU emulada não serve para ver o 3D neste PC. O caminho é o renderizador nativo, cujos shaders vêm do
XenosRecomp + DXC (outro tradutor).

## Renderizador nativo: primeira execução (2026-10-09)

Biblioteca de shaders (`tools/build_shader_library.sh out/shaders_in`, XenosRecomp + DXC 1.9.2607): dos 1.409
contêineres, 1.409 traduzidos para HLSL e **1.383 compilados para SPIR-V** (`ds2_shaders.mesp`, 61,4 MB, + `.idx`).
Os 26 que o DXC recusa:

| Quantos | Erro do DXC |
|---|---|
| 12 (vértice) | `partial explicit stage input location assignment via vk::location(X) unsupported` |
| 9 (pixel) | `use of undeclared identifier 'ToneMapTableSampler_TextureNDDescriptorIndex'` e `no matching function for call to 'getWeightsND'` (o mesmo grupo de shaders) |
| 4 | `expected unqualified-id` |

São defeitos do tradutor para construções que o SA2 não tinha (por exemplo um amostrador de textura 3D para a tabela
de tone mapping); enquanto não forem corrigidos, os desenhos com esses shaders ficam sem imagem.

Primeira execução com `ds2_renderer_native = true` no lavapipe (`out/pc/logs/ds2_010.log`, 78 s, até o aviso de
rede):

- Aviso de autosave, logos, tela-título e diálogos saem iguais aos da GPU emulada (comparação visual das folhas de
  contato, não pixel a pixel).
- Cerca de 16 quadros/s (911 trocas em 54 s), contra ~8 da GPU emulada em Vulkan no mesmo lavapipe.
- `[verify] ... differences: 0` em todas as linhas (6.390 autoverificações dos caminhos rápidos do anel).
- Os shaders do jogo são identificados pelos objetos (por exemplo VS `480B3420` → entrada n874). Ficam sem identificar
  quatro shaders internos do Direct3D, carregados por `IM_LOAD_IMMEDIATE` (VS de 15, 24 e 27 palavras, PS de 9
  palavras): são os de clear/cópia, que o guia manda embrulhar com `tools/wrap_discovered.py` a partir do despejo de
  descoberta. Ainda não feito; não se sabe o que deixa de ser desenhado por causa deles.

## Renderizador nativo no primeiro mapa 3D (2026-10-09)

Sessão `out/pc/logs/ds2_011.log` (renderizador nativo, lavapipe; Continue, vídeo pulado, carregamento):

- Vídeo de abertura e tela de carregamento saem certos (`video_100s.rgba`).
- Aos 305 s o processo cai com `0xC0000005`, o mesmo instante da sessão com a GPU emulada. O despejo
  (`ds2.exe.6344.dmp`) mostra a falha numa thread do rasterizador do lavapipe: leitura do endereço 0 em código JIT
  (fora de qualquer módulo), com só `vulkan_lvp.dll` na pilha. Nenhum quadro de `ds2.exe` nessa thread. **É uma queda
  dentro do driver por software ao desenhar, não do código traduzido do jogo**; o que o renderizador pediu de errado
  (ou o que o lavapipe não tolera) ainda não foi isolado.
- Últimas linhas do renderizador antes da queda: a cadeia de pós-processamento do 3D sendo criada (alvos de 640x360,
  64x64, 16x16 em formato 32, textura 256x1), e três avisos:
  - `unidentified pixel shader: 78 words, fingerprint 433CF019AE460530` (um shader que não está na biblioteca);
  - `VS identity mismatch 18 ... loaded_words=126 selected_n=496` (um vertex shader carregado que não bate com a
    entrada escolhida);
  - `signed textures not supported yet: read as unsigned` (formato 29).

O próximo trabalho é o ciclo de shaders do guia (seção 6.2): despejo de descoberta, variantes de vértice que o
Direct3D reescreve, os 26 shaders que o DXC recusa e os internos do Direct3D, até o log não ter carga sem
identificar. Para achar o desenho que derruba o lavapipe, as camadas de validação do Vulkan ajudariam.
