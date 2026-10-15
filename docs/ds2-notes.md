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

## Ciclo de shaders e primeiro quadro 3D do motor (2026-10-09)

**Tradutor (XenosRecomp).** Os 26 contêineres que o DXC recusava vinham de quatro defeitos, todos corrigidos em
`shaders/XenosRecomp` (comentários `ds2-nx` no código):

| Defeito | Shaders | Correção |
|---|---|---|
| amostrador com nome lido como textura 1D (`ToneMapTableSampler`, 256x1) sem índice 1D | 9 de pixel | o índice 1D é o 2D do mesmo slot, como já era para os `sN` sem nome |
| `getWeights2D` chamada com 1/tamanho, que só `tfetch2D` aceitava | 9 de pixel (filtro do mapa de sombras; em parte os mesmos acima) | sobrecarga que ignora a constante e consulta a textura |
| fetch cujo destino só recebe constantes: `r0. = tfetch2D(...).;` | 4 de pixel | nada é atribuído do fetch; só as constantes |
| entradas de vértice `TANGENT1` e `COLOR2` sem `vk::location` | 12 de vértice (modelos) | recebem as localizações de `BINORMAL0` (3) e `TEXCOORD4` (12), que nenhum deles usa; `LocationOfUsage` do renderizador igual |

Biblioteca refeita com os 1.409 do disco + 18 contêineres que o renderizador reconstruiu das cargas imediatas do
Direct3D (`shader_misses/missing`) + 1 shader interno embrulhado (15 palavras): **1.428 contêineres, 0 falhas, 1.417
shaders** (`ds2_shaders.mesp`, 62,5 MB). Nos menus os desenhos sem shader caíram de milhares para 1 a cada 10 s.

**Religação de interpoladores do Direct3D.** No 3D, o Direct3D do XDK 21256 carrega os vertex shaders dos modelos
como microcódigo imediato já alterado: além dos fetches reescritos para a declaração de vértices, ele troca por
instruções vazias (`C8000000 00000000 02000000`) as instruções ALU que exportam interpoladores que o pixel shader em
uso não lê (visto nas instruções 39 e 40 de um shader de 126 palavras: biblioteca `C8038001 00B0C600 A100FF00`). A
verificação de identidade recusava esses shaders (`reason=non-fetch-word`) e o desenho ficava sem vertex shader.
Agora ela aceita exatamente esse caso (`me_shader_identity.h`, chave `ds2_native_vs_removed_exports`): a instrução
carregada é a vazia, a da biblioteca é uma ALU executada (bit de sequência do fluxo de controle) que só exporta para
os registradores 0..15. O shader da biblioteca, sem a alteração, desenha a mesma imagem: só escreve saídas que
ninguém lê. **Não há autoverificação desse caminho** (não existe caminho antigo que desenhe para comparar).

**Resultado** (`out/pc/logs/ds2_014.log`, 435 s, sem queda): depois do carregamento o renderizador desenha a cena de
abertura de Things Betwixt, ~700 desenhos e ~60 cópias de alvo por quadro, 7 desenhos sem shader a cada 10 s, a
~0,5 quadro/s no lavapipe. `video_200s.rgba`, quadros 100–109: troncos, capim e o chão dentro das barras de cinema,
**muito escuros e com as cores erradas**. O que resta no log:

- 58 `identity mismatch`, 25 deles `word-count`: o registro do desenho aponta um objeto de shader de outro tamanho
  (o pareamento desenho ↔ objeto erra); a maioria é de pixel shaders de 9/15/18 palavras.
- `signed textures not supported yet: read as unsigned` (formato 29), na cadeia de pós-processamento.
- A queda dentro do lavapipe aos ~300 s apareceu em 2 das 4 sessões com o renderizador nativo que chegaram ao mapa
  (logs 011 e 013; não em 012 e 014), sem relação clara com as opções ligadas. Nas duas o endereço termina igual
  (`...1678`, leitura de 0, código JIT): parece o mesmo ponto. Causa não isolada.

## A cena 3D depois do fade-in (2026-10-09)

Sessão `out/pc/logs/ds2_017.log` (renderizador nativo, lavapipe, 675 s, sem queda; `video_300s.rgba`):

- A cena de abertura de Things Betwixt aparece inteira em forma (tronco em primeiro plano, capim, chão, ruínas ao
  fundo, barras de cinema), a ~0,6 quadro/s: ~700 desenhos e ~56 cópias de alvo por quadro, 7 desenhos sem shader a
  cada 10 s.
- **As cores estão erradas**: depois do fade-in a imagem satura em verde e amarelo (verde no máximo em quase tudo,
  vermelho no máximo em parte, azul zero). A geometria e a profundidade parecem certas; o defeito está na cadeia de
  cor (alvos HDR, cópias com conversão de formato, tone mapping). Pistas no log, nenhuma confirmada como causa:
  - alvos em formato 2 (2_10_10_10 em ponto flutuante, "7e3") copiados para textura de formato 7:
    `resolve conversion: guest RT 2 host 97 -> guest texture 7 host 64`;
  - texturas de formato 2 lidas pelo jogo (`C3: texture ... format 2 linear ... swizzle 124`);
  - `signed textures not supported yet: read as unsigned` (formato 29, 64x64, na cadeia de luminância);
  - a tabela de tone mapping, um alvo 256x1 desenhado a cada quadro e lido como textura 1D.
- 64 `identity mismatch` no log (pareamento desenho ↔ objeto de shader com tamanhos diferentes).

**A queda no lavapipe.** Sessões com o renderizador nativo que chegaram ao mapa: caíram 011, 013 e 016 (sempre aos
~295–305 s, na entrada do 3D); não caíram 012, 014 e 017. A 017 rodou com `ds2_native_textures_binding_thread =
false` (hipótese: um desenho usa o slot de uma textura nova antes de a thread de ligação escrevê-lo, o que numa GPU
real passa e no lavapipe é leitura de ponteiro nulo). Uma sessão sem queda não prova a hipótese: com metade das
sessões caindo, seriam precisas várias seguidas.

Sem um depurador de quadro não dá para ir muito além por leitura de log: o próximo passo natural é capturar um quadro
com o RenderDoc (o renderizador já tem o gancho, `me_renderdoc.cpp`; os portes de referência têm scripts `rdc_*.py`)
e conferir passe a passe, e usar as camadas de validação do Vulkan para a queda.

## A cadeia de cor do 3D, passe a passe (2026-10-09)

**Ferramentas.** O RenderDoc (1.46, em `_tools/renderdoc`) captura, mas não serve com este renderizador sobre o
lavapipe: sob a camada dele o dispositivo deixa de anunciar `bufferDeviceAddress` e o renderizador não desenha
(`C6: the Vulkan device does not have bufferDeviceAddress: nothing is drawn`). No lugar entraram dois diagnósticos
próprios, ligados ao rastro de quadro (`banjo_trace_frame_s`):

- `DS2_TRACE_DUMP=1` grava a textura de destino de cada cópia (resolve) do quadro rastreado em
  `out/pc/trace_dump/`, e `tools/trace_dump_png.py` faz os PNGs e a tabela de mínimo/média/máximo por canal;
- `DS2_TRACE_PEEK=<endereço>:<bytes>,...` mostra bytes da memória física do jogo no fim do quadro.

As camadas de validação do Vulkan estão em `_tools/vulkansdk` (SDK 1.4.363.0, instalado só por cópia de arquivos);
ligam-se com `VK_ADD_LAYER_PATH=_tools\vulkansdk\Bin` e `REX_VULKAN_VALIDATION_ENABLED=true`. Ainda não usadas.

**O que o quadro mostrou** (`out/pc/logs/ds2_024.log`, quadro depois da troca 1658, já em jogo, com o HUD):

- O alvo HDR da cena (`117AC000`, 1280x720) está **certo**: o personagem sobre a plataforma de pedra de Things
  Betwixt, pilares com hera, capim, a fenda azul no céu. Lido como 7e3, vermelho/verde/azul têm média
  0,12/0,17/0,23 e máximo 14/25/32.
- O jogo desenha a cena em 7e3 (formatos de alvo 3 e 12), copia o mesmo alvo declarando-o inteiro de 10 bits
  (formato 10) para uma textura 2_10_10_10 — os bits crus — e a lê com formato 54. O renderizador faz essa
  reinterpretação pelos bits (`EDRAM mode4 pair C000/f3 -> C000/f2`), como deve.
- A tabela de tone mapping (`111BE000`, 256x1, desenhada por um pixel shader a partir da luminância adaptada) saiu
  **toda zero**, e a composição final (`10220000`) é só o HUD sobre preto.
- A textura de luminância adaptada (`12556000`) vale (0,045; 0,13; 55,7; 1) na região escrita.

**Causa encontrada: `exp_adjust` das texturas.** O pixel shader final faz `floor()` e `frac()` do valor lido da cena
e usa a parte inteira como expoente e a fração como mantissa do 7e3. Isso só funciona se o fetch devolver o código
de 10 bits multiplicado por 8, que no Xbox 360 é o campo `exp_adjust` da constante de textura (a GPU multiplica o
valor amostrado por 2^exp_adjust). O renderizador não aplicava esse campo a texturas (só registrava um aviso para
fetches de vértice). Agora o expoente (6 bits) vai nos bits 16..21 da palavra que já levava o índice e os sinais da
textura, e os fetches do tradutor multiplicam por 2^expoente. **Verificado em execução** com a biblioteca refeita
(`ds2_026.log`): o rastro mostra `t0=117AC000 ... fmt 54 ... exp 3`, a redução de 640x360 da cena passou de média
~0,015 para ~0,12 e a luminância medida de 0,045 para 0,14.

Hipótese descartada no caminho: o limite `[FLT_MIN, FLT_MAX]` das instruções log/recíproco "com limite" do tradutor
parecia errado, mas `FLT_MIN` ali é definido como o maior negativo (`0xff7fffff`); está certo.

(Explicada na seção seguinte: era o resolve de 256x8 encolhido para 256x1.) A tabela zerada ainda não estava explicada por si só: pelo shader que a desenha, ela sai exatamente zero quando a
coordenada de textura chega zerada (ou a luminância lida é zero). Pode ser consequência da mesma causa (as texturas de
luminância são lidas com outro formato) ou um segundo defeito; a conferir depois da biblioteca nova.

## O 3D na tela: a tabela de tone mapping e a queda do lavapipe (2026-10-09)

Sessão `out/pc/logs/ds2_035.log` (renderizador nativo, lavapipe, 560 s, sem queda): a composição final do quadro
rastreado (`10220000`) e os quadros apresentados mostram o jogo — o personagem em Things Betwixt, pilares com hera, o
capim, a fenda azul, o HUD e a faixa com o nome da área. Duas causas, as duas no renderizador herdado e nenhuma no
código do jogo:

**1. A tabela de tone mapping saía zerada por causa de um resolve "encolhido".** O jogo desenha a tabela na linha 0 de
um alvo de EDRAM e copia uma janela de **256x8** para a textura de **256x1**. No console essa cópia é 1 para 1 (o que
passa do tamanho da textura cai no preenchimento do armazenamento dela). O renderizador tinha um caminho, feito para o
Mass Effect desenhado acima do tamanho do alvo, que **reduz** a origem para o destino com um blit linear sempre que a
janela pedida é 1,25 vez o destino ou mais: as 8 linhas viravam 1 e a única linha desenhada sumia (o log dizia
`[resolution] the 256x8 scene is downscaled to 256x1 when resolved (supersampling)`). Esse caminho agora só entra com
`ds2_native_resolve_shrink = true` (padrão `false`): a cópia é 1 para 1 e recortada. Depois disso a tabela vale
0,004–0,54 com alfa 1 e a composição final tem média 0,18/0,20/0,22 (antes 0,008/0,007/0,006, só o HUD).

O que foi descartado no caminho, com medição: constantes do pixel shader (`DS2_TRACE_PS_CONSTANTS`), enquadramento do
desenho de uma linha (`DS2_TRACE_THIN_DRAWS`: viewport 0,0 256x1, meio pixel certo), atributos de vértice do VS da
tabela (`DS2_TRACE_VS_ATTRIBUTES=<n>`: posição float4 no deslocamento 0, coordenada float2 no 16, passo 24) e a posse
dos blocos de EDRAM (o desenho publicava; só não havia mudança de dono para registrar).

**2. A queda dentro do lavapipe era um índice de textura fora do heap, por colisão de chave.** As quedas tinham estado
idêntico (mesmo código JIT, mesmos registradores, leitura do endereço 0 numa thread de rasterização), ou seja, um
desenho específico, não uma corrida. O código no ponto da queda carrega o ponteiro `functions` de um descritor de
textura e o encontra nulo. Um autoteste novo (`DS2_CHECK_SLOTS=1`: todo índice de textura e de amostrador entregue a um
desenho tem de estar dentro do heap e já escrito) apontou o desenho: o amostrador de cubemap do registro 9 (o mapa de
ambiente) recebia o slot 107 ou 110, com o heap de cubemaps tendo 64 entradas e um único slot alocado. Quem devolvia o
slot era `SlotView`, pela busca de uma vista existente.

A chave das vistas era `XXH3_64bits_withSeed(&imagem, 8, heap << 40 | swizzles)`. Para uma entrada de 8 bytes o XXH3 só
faz XOR da entrada com (constante − semente) antes da mistura final, então duas imagens cujos handles diferem em 0x200
(o bit 41 da semente cai no bit 9 do handle; 0x100 para 3D), com os mesmos swizzles, têm a **mesma** chave como vista
2D e como vista de cubemap. Handles são ponteiros que o driver aloca em sequência, daí acontecer em parte das sessões
e não em outras. A diferença de 0x200 entre os handles é dedução pela fórmula do XXH3; o que foi observado
diretamente é `SlotView` devolver 110 para o heap 2 com `next = 2` e lista livre vazia. Correção: a chave agora é o
hash dos quatro campos como dados (16 bytes), e um acerto de chave cuja imagem ou heap não confere usa a textura vazia
e avisa (causa 59). Numa GPU real isso não derruba nada: o shader amostra o que houver naquela posição — o defeito
existe nos portes de referência e valeria avisar lá.

Com isso cai a hipótese anterior (thread de ligação de texturas): as sessões 029, 031 e 033 caíram com
`ds2_native_textures_binding_thread = false`. As camadas de validação do Vulkan (sessão 032, 480 s, até o 3D) não
acusaram nenhum erro; só avisos de saída de fragment shader sem anexo de cor.

**Como fica o 3D no lavapipe** (i5-3210M, sem GPU): ~0,5 quadro/s em jogo, ~670 desenhos e ~28 cópias por quadro, 136
pipelines, 212 texturas. Serve para conferir imagem, não para jogar.

**Pendências que o log ainda mostrava, por quadro (as duas primeiras resolvidas na seção seguinte):** 3 desenhos sem shader identificado; 2 desenhos rejeitados (causa
109: textura copiada de 128x128 lida com outro tamanho lógico, `resolved logical crop rejected 14B11000 128x128`);
divergências de identidade no pareamento desenho ↔ objeto de shader (VS n505/n882, PS n838/n904); texturas com sinal
lidas sem sinal (formato 29).

Diagnósticos acrescentados: `DS2_TRACE_VERTICES=<bytes>` (os buffers de vértice pequenos de cada desenho rastreado),
`DS2_TRACE_VS_ATTRIBUTES=<n>`, `DS2_TRACE_PS_CONSTANTS`, `DS2_TRACE_THIN_DRAWS=1`, `DS2_CHECK_SLOTS=1`.

## Quadro completo no primeiro mapa: nenhum desenho rejeitado ou sem shader (2026-10-09)

Sessão `out/pc/logs/ds2_037.log` (560 s, sem queda, `DS2_CHECK_SLOTS=1` sem nenhum erro): no quadro rastreado, 673
desenhos, **0 rejeitados e 0 sem shader** (antes 2 e 3 por quadro); o céu ganhou as estrelas que faltavam. Duas
correções, as duas pendências da seção anterior:

**Texturas comprimidas em endereços que já foram destino de cópia.** Durante o carregamento o jogo copia alvos
pequenos para texturas (256x152 em `14AE9000`, 128x76 em `14B11000`), libera essa memória e carrega nela texturas DXT1
do mapa (128x128 em `14B09000` e `14B11000`). O renderizador ainda tinha o registro da cópia antiga, tratava o fetch
como leitura de textura copiada, o tamanho não batia e o desenho era rejeitado (causa 109). Uma cópia (resolve) nunca
produz dados comprimidos em blocos, então um fetch de formato DXT agora é sempre lido da memória do jogo
(`ds2_native_compressed_fetch_not_resolved`, padrão `true`).

**Vertex shaders com os fetches reordenados entre dois streams.** O Direct3D do jogo reescreve os fetches de vértice do
shader para a declaração de vértices em uso e os agrupa por stream. A prova de reordenação herdada do sa2-nx
(`me_vertex_fetch_reorder.h`) só aceitava um fetch que se movesse entre bases equivalentes (mesmo stream); a geometria
do mapa usa dois streams (passos de 20 e 28 bytes) e o fetch de r2 sai do fim da ordem da biblioteca para o grupo do
primeiro stream: `DECL permutation crosses inequivalent actual full/mini bases`, e o desenho ficava sem vertex shader.
A condição existia porque a ferramenta offline que a prova copia conserta um contêiner que depois é lido na própria
ordem; em execução o caminho de desenho tira buffer, passo, deslocamento e formato de cada elemento do slot em que o
programa **carregado** o tem, e a base de um mini fetch do fetch completo anterior no programa carregado. Agora só se
exige o que a ordem poderia mudar: a mesma fonte de índice nas duas bases e nenhum fetch entre elas escrevendo essa
fonte (`ds2_gpu_vs_fetch_reorder_streams`, padrão `true`). A mesma prova passou a aceitar, junto com a reordenação,
os exports removidos pelo relink de interpoladores. Identificados assim: VS n33 (261 palavras, 10 fetches) e VS n939
(138 palavras, 7 fetches). O teste de host `shaders/tests/test_vertex_fetch_reorder.cpp` (do sa2-nx, contra o pacote
do SA2) não foi refeito para a condição nova.

O log ainda registra divergências de identidade no pareamento por endereço do buffer de comandos (um objeto de shader
errado é escolhido primeiro e descartado pela conferência do microcódigo); todos os desenhos acabam identificados.

## Lapidação no PC: o que mais o log e a validação mostraram (2026-10-09)

- **Resolução de render abaixo de 720p não funciona neste jogo (bloqueada).** Com `ds2_render_height_handheld = 360`
  (tentativa de acelerar os testes no lavapipe) o processo caiu com corrupção de heap no primeiro quadro 3D. A camada
  de validação mostrou a causa: `vkCreateImage(): extent.height is zero`. A escala do sa2-nx converte a borda de um
  pixel do jogo para o host pela regra do centro do pixel; a q/4 = 1/2 a borda da linha 1 é 0, e os alvos de uma linha
  do jogo (a tabela de tone mapping de 256x1) e os de 1x1 ficam com altura 0. As alturas de render foram então limitadas a
  720–2160. Depois (2026-10-10) o mínimo passou a 540, onde a mesma conta dá altura 1: ver "Rodada 3" no fim.
- **`signed textures not supported yet` era alarme falso** no caso do jogo: são os alvos de luminância em ponto
  flutuante (formato 29, lido pelo host como 16F), que já têm sinal. O aviso ficou só para formatos inteiros.
- **A "diferença" que a linha `[verify]` conta (1)** é o `C6 prewarm: MISMATCH`: no lavapipe criar um pipeline que já
  foi pré-aquecido leva tanto quanto compilar (não há cache de binários), e a thread de pré-aquecimento se desliga.
  É do driver por software, não do jogo; a conferir de novo no Switch.
- **Pareamento desenho ↔ registro de `Draw*`:** `draws paired with a Draw* record 0`. Todos os desenhos chegam dentro
  de buffers indiretos e nenhum casa com os registros dos três ganchos de `Draw*`; o pareamento cai no endereço do
  buffer de comandos (que às vezes aponta um objeto de shader antigo, daí os `identity mismatch`) e a identificação
  final é pelo microcódigo. A imagem não depende disso; é custo de CPU na thread do anel, a tratar junto com as
  otimizações de CPU.
- **Validação do Vulkan** numa sessão inteira até o 3D em 720p (`ds2_032.log`): nenhum erro.
- O jogo guarda o progresso em `Documents\ds2\<perfil>\465307E4\00000001\-GAME_0000` (23 arquivos, 7,2 MB), aberto a
  cada sessão: as sessões de teste continuam de onde a anterior salvou.

## Percorrer o jogo num PC sem GPU: ferramentas e o que foi coberto (2026-10-09)

**O problema.** No lavapipe o mapa 3D sai a ~0,5 quadro/s, e o jogo avança por quadro: a essa taxa ele roda em câmera
lenta (o personagem levou ~5,5 min de relógio para correr da plataforma inicial até a primeira rocha). Roteiros por
tempo não servem para andar pelo mapa, e não dá para jogar à mão.

**Ferramentas novas:**

- `ds2_diag_draw_every = N` (diagnóstico): só um quadro em N é mandado para a GPU; nos outros a thread do anel lê
  todos os pacotes, identifica os shaders e confere os desenhos, mas não envia desenho, limpeza nem cópia. Com N = 61 o
  jogo roda a 10–12 quadros/s no mapa (20 vezes mais rápido) e a imagem se atualiza a cada ~5 s. N ímpar para os
  alvos alternados do jogo (luminância adaptada) continuarem alternando. No primeiro quadro 3D aparece o aviso `texture
  format not supported yet (cause 429)`: é a textura de luminância lida antes da primeira cópia, por causa dos
  quadros pulados.
- `ds2_input_live = <arquivo>`: controle ao vivo. O jogo relê o arquivo quando ele muda; itens `<botões> [segundos]`
  separados por vírgula (`lup, a 0.6`).
- `ds2_snap_live = true`: captura sob demanda (`captures/snap_request` → `captures/snap.rgba`).
- `tools/live.py`: `pad "<itens>"`, `snap [nome]`, `wait <s>`, `sheet a b c` (uma folha com várias capturas), para
  guiar o jogo passo a passo numa sessão longa (lançada à parte com `Start-Process`, pois dura mais que um comando).
- `audio_dump_output_s` agora grava também com `audio_mute` (as sessões de teste são mudas): **única diferença do
  `sdk/` em relação ao sa2-nx**, em `sdk/src/audio/sdl/sdl_audio_driver.cpp`, marcada com `ds2-nx`.

**O que foi coberto e conferido por imagem:**

- Do boot ao jogo: avisos iniciais, logo da Havok, tela de título, "Now loading" do save, menu principal (New Game /
  Continue), telas de carregamento com descrição de item, fade-in do mapa. Tudo certo.
- Sessão de 25 min a 720p andando pelo capim de Things Betwixt (`ds2_040.log`, todos os quadros desenhados): 0
  desenhos rejeitados, 0 sem shader, 0 erros de slot, sem queda. Capim, rochas, HUD e a faixa da área certos.
- Sessão guiada de 22 min com 1 quadro em 61 (`ds2_042.log`): 14.766 quadros do jogo, 0 rejeitados, 0 sem shader,
  183 pipelines, 404 texturas. O personagem caiu num abismo, **morreu, o jogo recarregou e ele renasceu** na
  plataforma inicial: morte, telas de carregamento e recarga do mapa funcionam.
- **Áudio** (70 s gravados da tela de título e dos menus, `out/pc/audio_output.wav`): 24 s de silêncio nos avisos
  iniciais, depois sinal contínuo, pico 0,29, sem saturação, sem NaN, estéreo (L ≠ R), com um pulso a cada 6 s que
  coincide com os toques de A do roteiro (som de menu). Parece áudio de verdade; a qualidade só se julga ouvindo.

**Onde parou.** Saindo da plataforma em linha reta o personagem bate numa rocha depois do capim; contornando para um
lado há um abismo, para o outro ele voltou a encostar na mesma rocha. Com uma imagem a cada ~5 s e a câmera voltando
para trás do personagem, cada passo "olhar e agir" leva mais de um minuto; achar a passagem para a cabana (onde ficam
a cena e a criação do personagem) é questão de horas neste PC. Não foi além da área inicial.

## Build para o Switch: o que esta máquina precisou (2026-10-10)

Decisão do usuário: o PC (Vulkan por software) é fraco demais para seguir testando; os testes passam para o console.

**O jogo (`tools\win\build_switch.ps1`).** O devkitPro daqui fica em `Downloads\devkitPro` e a variável `DEVKITPRO` do
sistema vale `/opt/devkitpro` (caminho do MSYS2). O CMake relê o arquivo de toolchain dentro dos testes de compilador,
onde `-DDEVKITPRO` não chega, e a configuração falhava com `libnx linker specs not found: /opt/devkitpro/libnx/
switch.specs`; o script agora exporta o caminho do Windows. A configuração também exige o driver
(`REXGLUE_SWITCH_NVK_SDK` com `lib/libvulkan.a`); para compilar os objetos enquanto o driver não existe, serve uma
pasta com um `libvulkan.a` vazio (`out\nvk_stub`), e no fim reconfigura-se com o SDK de verdade (só o link refaz).
Sem LTO nem PGO nesta fase (o `app/CMakeLists.txt` ainda não os liga).

**O driver (NVK, Mesa).** Não há binário pronto: compila-se
[danfromtico/mesa-switch](https://github.com/danfromtico/mesa-switch) no commit `1a8c1a66d6f` com o patch do nfsmw-nx
(`_ref/nfsmw-nx/mesa/mesa-switch-nfsmw.patch`), em `_tools/mesa-switch` (fora do git). Os scripts foram escritos para
um MSYS2 em `C:\msys64`; o que foi instalado e ajustado aqui:

- MSYS2 em `C:\msys64` (winget `MSYS2.MSYS2`), e `C:\msys64\opt\devkitpro` como junção para `Downloads\devkitPro`;
- pacotes: `flex bison git make patch diffutils pkgconf`; do MINGW64 `gcc meson ninja python python-mako python-yaml
  python-packaging rust-bindgen pkgconf`; do CLANG64 `clang llvm lld libclc spirv-llvm-translator spirv-tools cmake
  pkgconf cbindgen zlib zstd` (as ferramentas de host do Mesa são compiladas com o clang do CLANG64);
- no MSYS2 do devkitPro: `switch-libexpat` e `switch-libzstd` (o Mesa precisa deles; não vinham instalados);
- Rust por `rustup-init` dentro de `_tools/mesa-switch/build/deps/{cargo,rustup}` (onde os wrappers o procuram):
  `stable-x86_64-pc-windows-gnu` com o alvo `aarch64-unknown-linux-gnu`;
- `switch_native_tools_msys2.txt`: `cbindgen` em `clang64` (o arquivo apontava para `clangarm64`, a máquina do autor);
- `windows_x64_host_cross_file_msys2.txt`: a entrada `cmake` aponta para um executável que não existe. Com o CMake
  4.4 e o Windows em português, o Meson 1.12 quebra ao consultar o CMake numa dependência opcional (`libudev`:
  `'utf-8' codec can't decode byte 0xba`, "This is a Meson bug"); nada nesse passo precisa do CMake;
- execução, num shell MINGW64: `PATH=$PATH:/clang64/bin DEVKITPRO=C:/msys64/opt/devkitpro ALLOW_DIRTY=1
  ./build-unified.sh` (com `NINJA` apontando para um wrapper `ninja -j2`, para dividir a máquina com o build do jogo).

**Primeiro NRO (2026-10-10).** `out\nx\ds2.nro`, 105 MB, codegen clássico, sem LTO nem PGO. Tempos neste PC: ~2 h 15 min
para os 938 passos do jogo (4 threads, dividindo a máquina), ~35 min para o Mesa com 4 threads. `tools/check_switch_build.py`
(adaptado ao DS2) passa:

- `_start` no endereço 0; o NRO leva o driver recém-compilado (`Mesa 26.2.1 (git-1a8c1a66d6)`, igual ao do SDK);
- 5 desvios para si mesmo em funções recompiladas (`sub_828FC278`, `sub_82B11CB8`, `sub_82BE8748`, `sub_82C3FAA0`,
  `sub_82DE5A18`). A imagem do jogo não tem nenhum `b .`; são laços do código original que não terminam para uma
  entrada que o jogo não produz e que não leem memória nesse caminho (uma busca em lista que continua comparando um nó
  nulo; um preenchimento cujo contador não é relido). O GCC prova que não terminam e emite `b .`. Dois foram lidos no
  código gerado; os outros três têm a mesma forma no desmonte. Não são laços de espera sem barreira;
- 2 `fmla` vetoriais em `sub_82C31A30`: são dois `fmsubs` escalares do próprio jogo (há 24 seguidos na função) que o
  GCC juntou numa instrução de duas pistas `double`; não é contração que vazou (a aritmética VMX é de precisão simples).

O link sem o driver acusava também `posix_memalign` (usado pelo FFmpeg): quem o define no console é o próprio
`libvulkan.a` do mesa-switch (`rust_switch_stubs.c`).

**Cartão SD.** O cartão do usuário é FAT32: o jogo vai como pasta `game_root/` solta (121 arquivos, 5,71 GB, o maior
com 2,64 GB), sem pacote único. `tools\win\package_switch.ps1 -IncludeGame -LooseGame` monta
`out\nx\sdcard\switch\ds2-nx` (129 arquivos, 5,87 GB); instruções em `docs/switch-testing.md`.

## Primeiro boot no Switch (2026-10-10)

Logs em `out/switch_logs/001` (fora do git): `ds2_001.log`, `rex_profile.log`, `rex_stderr.log`. Console em modo
portátil, CPU a 1020 MHz, GPU a 460,8 MHz (o gerente de relógios do SDK sobe a GPU de 307 para 460 MHz).

- **Funcionou de primeira:** 3 min 18 s de sessão, do boot ao primeiro mapa (New Game) e à saída pelo menu, sem
  queda. Relato do usuário: imagem certa do começo ao fim, exceto a caixa de confirmação de saída, rosa/roxa.
- **Renderizador:** 417.049 desenhos, **0 rejeitados, 0 sem shader**, 192 pipelines, 251 texturas, 3 vertex shaders
  identificados pela reordenação de fetches. Nenhum aviso do renderizador no log.
- **Som:** 471 de 471 buffers de áudio com sinal no mapa, pico 0,19, nenhum saturado.
- **Save:** o jogo criou e regravou o save no cartão (`ds2/B13EBABEBABEBABE/465307E4/00000001/-GAME_0000`, 23
  arquivos).
- **Taxa de quadros:** menus e telas 2D a 43–56 quadros/s; no mapa 3D **5–7 quadros/s** (até 13 conforme a vista).
- **O limite no 3D é a GPU:** `GPU per Swap: total ~150 ms`; a thread do anel passa ~80 % do tempo esperando a GPU
  (`fences waited 46/s, 805 ms/s`) e usa 19 % de um núcleo. Por quadro: ~606 desenhos, ~28 cópias, e as
  transferências de EDRAM (por 10 s: `color alias=402 depth import=636 guestspace depth resolve=804`, e 8,5
  conversões de representação do alvo de profundidade inteiro por quadro). As categorias de tempo de GPU estavam
  desligadas nesta sessão; o detalhe por passe sai com `configs/ds2_perfil.toml`.
- **CPU:** a thread principal do jogo fica em 92–96 % de um núcleo tanto nos menus (50 quadros/s) quanto no mapa
  (6 quadros/s), ou seja, gira esperando em vez de dormir; o perfil conta ~38.600 `svcSleepThread` de até 1 ms por
  segundo. A ver com a amostragem de pilhas.
- Este NRO não tem LTO nem PGO, e nenhuma das otimizações específicas por jogo dos portes de referência.

**A caixa "Save and quit game" roxa (corrigido).** Reproduzido no PC com o controle ao vivo (Start, ícone de sistema,
Quit Game): a caixa saía roxa, o botão selecionado rosa, e o HUD desenhado depois dela tingido. O rastro do quadro
(agora disparável a qualquer momento: `DS2_TRACE_LIVE=1` + `tools/live.py trace`) mostrou todos esses desenhos com
entradas certas: mesmo par de shaders (VS n505, PS n838: `textura * g_MulColor + g_AddColor`, vezes a cor do vértice),
cores de vértice brancas e `c32 = (1,1,1,1)`. As texturas, extraídas dos dados do jogo (`DS2_DUMP_TEXTURE=<bases>` +
`tools/bc_to_png.py`), são couro cinza-escuro, molduras azuis e brilho laranja: nada roxo. O roxo vem de **um** desenho
do HUD que o próprio jogo tinge com `c32 = (0,77; 0,29; 0,90; 0,69)`, em mistura aditiva.

A causa estava na deduplicação de constantes herdada (`ds2_native_constants_dedupe`): quando uma geração nova de
constantes tem os bytes que o shader lê iguais aos do último bloco enviado, o bloco é mantido. Mas ele continuava
marcado como válido no tamanho antigo inteiro. Sequência do jogo em todo quadro de interface: quadro roxo (o shader lê
até c33; bloco enviado com c32 roxo), texto (o shader do texto lê menos registradores; o prefixo é igual: bloco
mantido e dado como atual), demais quadros (c32 branco nos registradores, mesma geração: bloco roxo reaproveitado).
Correção: ao manter um bloco, ele passa a valer só até o tamanho comparado (para VS e PS). O mesmo defeito existe no
sa2-nx; lá não apareceu porque os shaders daquele jogo não combinam assim.

Verificado no PC: caixa cinza-escura, YES azul, NO laranja, contador de almas normal. O NRO foi refeito com a correção.

## Segundo e terceiro testes no Switch (2026-10-10)

Logs em `out/switch_logs/002` (sessões `ds2_004` e `ds2_005`, já com o NRO corrigido).

- O usuário confirmou no console: a caixa "Save and quit game" saiu certa.
- **Sem overclock o mapa fica em 6,8 quadros/s, estável** (68 trocas a cada 10 s durante minutos; `GPU per Swap:
  total 148,5 ms`; ~608 desenhos por quadro). Os 9–13 quadros/s do primeiro log eram de um overclock leve que o
  usuário testou: desconsiderados.
- As categorias de tempo de GPU não estavam ligadas: o jogo só lê `ds2.toml`, e o `ds2_perfil.toml` foi copiado ao
  lado sem substituir. O `ds2.toml` de teste passou a trazer `ds2_gpu_marks_categories = true`.
- **Bloqueio de progresso:** mais adiante o jogo pede o nome do personagem (`XamShowKeyboardUI`), e o diálogo do SDK é
  uma caixa ImGui que precisa de teclado físico. No Switch a chamada agora abre o teclado do sistema (libnx `swkbd`),
  numa execução "headless" do SDK que devolve o texto ou `X_ERROR_CANCELLED`. Segunda diferença do `sdk/` em relação
  ao sa2-nx. Não testado em console ainda; no PC nada muda.
- Ícone do NRO: `assets/icon.jpg` (fora do git; 256x256 JPEG fornecido pelo usuário), usado pelo CMake quando existe.

## Primeiro perfil de GPU no console (2026-10-10)

Sessão `out/switch_logs/003/ds2_006.log` (9,5 min; criação do personagem com o teclado do sistema, que funcionou:
`XamShowKeyboardUI: software keyboard confirmed (7 of at most 13 characters)`; depois a primeira fogueira, parado).

**Na fogueira: 6,3 quadros/s, `GPU per Swap: total 158,3 ms`**, ~980 desenhos, ~80 trocas de alvo e ~60 cópias por
quadro. Por categoria (ms por quadro): cena 34,0; alvos de 640: 6,1; cópias 3,9; cena sem Z 2,7; intervalos 10,6;
`edram_import` 35,0; `edram_alias` 50,1; `edram_import9` 15,1. As três últimas são passes que **começam** com uma
transferência de EDRAM; o tempo inclui os desenhos do jogo que vêm depois no mesmo passe. Pelo tempo por passe
(soma de 10 s, 63 quadros), o que é só transferência, sem desenho do jogo:

| Passe | ms por quadro | O que é |
|---|---|---|
| cat 10, sem shader | 21,5 | importações de profundidade (troca de representação do alvo inteiro, ~8 por quadro) |
| cat 13, sem shader | 9,3 | importações de stencil em 9 passes (~4 por quadro) |
| cat 12, sem shader | 1,5 | conversões de cor entre vistas do mesmo EDRAM |
| cat 12, PS n720 | 22,6 | desenhos do jogo (2 passes de tela cheia com um shader de 3,2 KB) |
| cat 2, PS n95 / n1125 | 10,0 / 6,6 | cena |

Os 16 passes listados somam 56 % do tempo; o resto está espalhado em dezenas de passes pequenos (a lista passou a ter
64 entradas). Ou seja, ~32 ms por quadro (20 %) são pura emulação de EDRAM, e o resto é o custo do próprio quadro do
jogo neste hardware a 720p com a GPU a 460 MHz.

**ZCULL desligado no alvo de profundidade principal.** O driver registra `ZCULL: SIN plano para profundidad
1280x1280: usos incompatibles (TRANSFER_DST?)`, e o mesmo para os mapas de sombra (800x800, 1600x1600, 1040x1040).
O renderizador dá `TRANSFER_DST` às vistas de profundidade de 1 amostra para importar stencil pelo motor de cópia
(`ds2_native_edram4_stencil_copy`), e isso custa o plano de ZCULL. No SA2 a profundidade da cena é multiamostrada e
não cai nessa regra; no DS2 ela é de 1 amostra, então a cena inteira roda sem descarte hierárquico. Teste A/B
preparado: `configs/ds2_teste_zcull.toml` (`ds2_native_zcull_1x_depth = true`).

**Cópia rejeitada nova:** `resolve exponent bias / format not supported (cause 14)`, a partir da área da fogueira
(1.226 cópias rejeitadas de 112.297 na sessão). O caminho com viés de expoente só cobre FP16 → FP16; o log agora diz
os formatos. Sem relato de defeito visível, mas é conteúdo que não chega à textura.

## Rodada 1 no console: ZCULL na profundidade de 1 amostra e a cópia com viés (2026-10-10)

Logs em `out/switch_logs/004` (`ds2_007` = A, `ds2_008` = B), mesma cena (primeira fogueira, parado), sem overclock.

| | A: padrão | B: `ds2_native_zcull_1x_depth = true` |
|---|---|---|
| quadros/s | 7,3 | 7,9 |
| GPU por quadro | 137,9 ms | 127,2 ms |
| `edram_import` | 35,0 | 20,0 |
| `edram_import9` | 15,1 | 19,0 |
| `edram_alias` (inclui os desenhos da cena que vêm depois) | 50,0 | 50,1 |
| cena | 23,4 | 23,6 |

O ganho veio das importações de profundidade, não da cena: os passes da cena não mudaram, ou seja, o custo deles é de
preenchimento/shader, não de pixels escondidos. Com ZCULL as importações de stencil deixam o motor de cópia e voltam
aos 9 passes (+3,9 ms), e ainda assim o saldo é −10,7 ms. Adotado no `ds2.toml` de teste.

Leitura da lista de passes (pesos relativos dentro de cada categoria; os valores absolutos dessa lista não fecham com
os das categorias, que são os calibrados): em `edram_alias`, ~70 % é o PS n720 em dois passes (~35 ms por quadro: é o
material principal da cena, não uma transferência); `edram_import` (20 ms) e `edram_import9` (19 ms) são só
transferência: **~39 ms por quadro, 31 % do tempo de GPU, em conversões que o jogo original não faz.** No PC, o
relatório de pares mostra o que são as de 9 passes: importações de cor para profundidade de poucos blocos (8 a 48)
quando o mapa de sombras e outros alvos reutilizam o início do EDRAM onde a cena desenhou cor.

**Cópia com viés de expoente.** O log do console deu os formatos: viés −2, do alvo 7e3 (formato 12, FP16 no host,
1280x1280) para a textura `10951000` RGBA8 1280x720, comando 1. É a cópia da cena para 8 bits com fator 1/4, lida pelo
PS n723 num desenho de tela cheia (o efeito de calor da fogueira). Feita agora pelo passe de fragmento das cópias
(`ds2_resolve_copy.frag` ganhou um fator; 1,0 para a cópia de sempre). Verificado no PC **com o save do console**
(`logs/ds2` copiado para `Documents\ds2`; o save anterior do PC está em `out/pc_save_backup`): a área da fogueira
abre pelo Continue, o log registra a cópia e a textura sai com a cena.

## Rodada 2: as limpezas por superfície 4x apelidada (2026-10-10)

Console, build da rodada 1 (`out/switch_logs/005/ds2_009.log`): 7,4 quadros/s, 136,4 ms de GPU por quadro na fogueira,
nenhuma cópia rejeitada (a cópia com viés agora roda; o passe que a lê custa ~9 ms).

**De onde vêm as importações.** Rastro de um bloco de EDRAM no PC (`ds2_diag_trace_tile`), na mesma cena, com o save
do console. Em todo quadro o jogo usa o Clear do Direct3D por uma superfície 4x de metade do passo, que apelida os
mesmos blocos do alvo de 1 amostra:

1. profundidade do mapa de sombras (base 0, 800 de passo), só profundidade;
2. cor em formato 3 (7e3, o alvo HDR de 640x360) + profundidade em 0xB8;
3. cor em formato 0 (o alvo de 1280x720) + profundidade em 0x2D0;
4. outras profundidades de meia resolução (0x658, 0x710).

O renderizador tem o atalho do sa2-nx ("redirected clear": limpar direto no dono de 1 amostra), mas ele recusava
esses casos, desenhava a limpeza na vista apelidada e depois convertia o alvo inteiro para a vista de 1 amostra:
uma importação de profundidade (com ZCULL, em 9 passes quando o dono anterior era cor) e uma conversão de cor por
limpeza. Motivos da recusa, pelo relatório `depth clear redirect declined by`:

- `also writes color (color 0 not redirectable)`: a metade de cor só aceitava formato 0 com dono RGBA8. No DS2 o
  começo do EDRAM é reutilizado por alvos de 8 bits e de ponto flutuante, então o dono dos blocos quase nunca serve;
- `target C.../f0 ... (clear of ... stencil -1)`: limpeza só de profundidade sobre blocos de um dono de cor, sem a
  vista de 1 amostra conhecida. Ela só era aprendida pelo ponteiro da vista de origem da importação, e no DS2 essa
  origem é uma vista com grade de raster (`msaaXY=0,1 gridX=1`), outro objeto que não o da vista 4x simples que o
  atalho consulta.

**O que mudou** (`ds2_native_edram4_clear_color0_alias`, padrão `true`):

- a vista de 1 amostra de uma vista 4x de cor é aprendida onde os blocos são transferidos, e a metade de cor da
  limpeza vai direto para ela, que passa a ser a dona dos blocos, quando o dono atual não pode recebê-la;
- o formato 3 é aceito com qualquer cor: o valor gravado é o que o próprio desenho da limpeza deixaria na imagem
  RGBA16F (saída limitada como em todo desenho num alvo 7e3: RGB em [0; 31,875], alfa em [0; 1]), arredondado para o
  half mais próximo. A cor de limpeza do alvo HDR do jogo não é zero;
- um retângulo que termina no meio de um bloco (os alvos de 640x360: 22,5 fileiras) é aceito quando o dono do bloco
  parcial é do mesmo formato;
- a vista de 1 amostra de uma profundidade é lembrada também pela geometria da superfície (base, formato,
  codificação, passo em blocos).

**Medido no PC** (lavapipe, 4 quadros desenhados por janela): conversões de profundidade do alvo inteiro, de 6 para 2
por quadro (1.924 → 352 blocos); a importação em 9 passes dos 720 blocos da profundidade principal sumiu; 896 blocos
de cor por quadro limpos direto (676 entregues à vista de 1 amostra). Imagem igual (captura na fogueira). Ainda
recusados por quadro: 1 limpeza de cor e 2 de profundidade em que o bloco parcial da última fileira pertence a uma
vista de outro formato (os alvos de 640x360); o caminho seria sincronizar só esses blocos antes de limpar.

**Medido no console** (`out/switch_logs/006`, fogueira, parado, sem overclock; as duas sessões no mesmo build):

| sessão | atalho | quadros/s | GPU por quadro | edram_alias | edram_import | edram_import9 |
|---|---|---|---|---|---|---|
| `ds2_010` | ligado (padrão) | 12,7 | 78,8 ms | 30,0 | 5,8 | 3,9 |
| `ds2_011` | `ds2_native_edram4_clear_color0_alias = false` | 7,9 | 127,9 ms | — | — | — |

Na sessão com o atalho: cena 23,7 ms, alvos de 640: 5,0, cópias 4,8, cena sem Z 2,6. Nenhum desenho ou cópia
rejeitado, nenhum defeito de imagem relatado. Sobram por quadro no console: a limpeza de cor com bloco parcial de outro
dono e duas importações em 9 passes de 8 blocos cada (`C000:1280x1280 → 000:800x800` e `→ 0B8:640x720`).

Curiosidade relatada: o menu New Game/Continue fica a 55–56 quadros/s na primeira vez e a 60 depois de carregar o
jogo e voltar. O perfil mostra a thread principal do jogo perto de 97 % de um núcleo nos dois casos (o menu é
limitado pela CPU do jogo, não pela GPU) e o pré-aquecimento de pipelines termina em 0,9 s; a causa da diferença não
foi determinada.

## Rodada 3: 540p no modo portátil (2026-10-10)

A escala de render do sa2-nx (`ds2_render_height_handheld`, q/4 do tamanho do jogo) estava bloqueada abaixo de 720p
porque a 360p (q = 2) os alvos de uma linha ficam com altura 0. A 540p (q = 3) a borda da linha 1 é
`ceil(3/4 − 1/2) = 1`, então o mínimo passou a 540 (`QuartersOf` em `app/src/me/ds2_render_mode.cpp`).

**Primeira tentativa no PC: cena inteira em ruído amarelo/verde, interface certa.** Sem queda e sem erro novo de
validação. O despejo das texturas do quadro (`DS2_TRACE_DUMP`, agora em pixels do host: antes lia o tamanho do jogo
de uma imagem menor) comparado ao de 720p mostrou uma única etapa diferente: a tabela de tone mapping
(`111BE000`, 256x1 → 192x1) tinha conteúdo velho do EDRAM em vez da rampa de cinza. Causa: `ViewportAndScissor`
(`ds2_native_draws.cpp`) descarta desenhos com viewport de menos de 1 pixel de altura, e o viewport de 256x1 do jogo
vira 192x0,75 no host. Um viewport assim ainda cobre o centro do pixel da linha 0; o limite agora é
`min(1, q/4)` pixels do host (um pixel do jogo abaixo de 720p; igual a antes em 720p ou mais). O descarte não
aparecia em contador nenhum ("0 rejected").

**Depois da correção:** imagem certa a 960x540 no PC (cena, HUD, texto, tabela de tone mapping idêntica à de 720p),
mesmas três mensagens de validação de antes, `DS2_CHECK_SLOTS` limpo. O recorte da mesma região escura a 540p e a 720p
não mostra padrão novo (as camadas de neblina de meia resolução aparecem nos dois).

O `ds2.toml` do pacote da rodada 3 vai com `ds2_render_height_handheld = 540` para medir no console;
`configs/ds2_teste_720p.toml` volta à resolução do jogo. A interface também é desenhada a 540p e ampliada (texto
menos nítido): se vale a troca é decisão de quem joga.

**No console a cena continuou amarela** (`out/switch_logs/007/ds2_012.log`, foto de quem jogou; HUD certo). A correção
acima só vale no lavapipe. O NVK programa o retângulo de recorte do viewport do hardware com campos inteiros
(`nvk_cmd_draw.c`: `.y0 = ymin`, `.height = ymax - ymin`, floats truncados): 0,75 de altura vira 0 e nada é
desenhado; de quebra, um viewport de 33,75 pixels perde a última linha (os alvos de 96x45 do bloom viram 72x34).

Solução no renderizador (`ds2_native_viewport_whole_pixels`, padrão `true`, em `ComputeFraming`): fora da escala do
jogo, um viewport com borda fracionária passa a ser os pixels inteiros do host cujos centros ele cobre (a mesma regra
de `me_resolution_scale.h`) e a diferença vai para `g_NdcScale` / `g_NdcOffset`, que todo vertex shader aplica no
espaço de clip. A geometria cai nas mesmas coordenadas do host; o meio pixel do D3D já é calculado a partir do
viewport enviado. Para a tabela: `0..192 x 0..0,75 → 0..192 x 0..1`, escala NDC `1 0,75`, deslocamento `0 0,25`.
No PC a imagem ficou igual à do viewport fracionário e a tabela é a mesma rampa. O efeito no NVK só o console confirma.

**Desempenho a 540p no console, ainda sem número firme.** Na mesma sessão (imagem errada, ~80 s dentro do mapa, sem
overclock) as janelas de 10 s deram 13,3 / 13,7 / 13,8 / 12,2 / 15,7 / 17,2 / 17,0 quadros/s com 76,2 / 73,2 / 72,5 /
82,5 / 63,4 / 58,1 / 58,6 ms de GPU por quadro: não estabilizou como a 720p (127 quadros por janela, 78,8 ms, em 17
janelas seguidas). Nas duas últimas: cena 22,4 ms (quase igual a 720p), edram_alias 17,1 (era 30,0), 640: 4,7,
cópias 2,9, edram_import 3,3, edram_import9 2,2. A comparação válida fica para a sessão com a imagem certa.

**Oitavo build no console** (`out/switch_logs/008/ds2_013.log`): cores certas, ou seja, o viewport de pixels inteiros
resolveu a tabela no NVK. Mas quem jogou viu "fantasmas": uma segunda silhueta do personagem deslocada para baixo e
à direita, e a cena com aspecto duplicado.

**Causa: a posição do pixel lida pelos pixel shaders.** O tradutor inicia o registrador de posição com
`iPos.xy - 0.5`, em pixels do host. Os passes de tela cheia do jogo (a máscara de sombra/luz `11EDE000`, entre
outros) multiplicam essa posição pelo `1/tamanho` do jogo para obter a coordenada de textura; a 540p a posição vai
só até 960x540, a coordenada até 0,75, e a entrada sai ampliada em 4/3 a partir do canto superior esquerdo. O sa2-nx
não cobre isso na escala de render (o SA2 não lê a posição). Estava errado também no PC: a comparação etapa a etapa
dos despejos de 540p e 720p (`out/view/s540_a.png` / `s720_a.png`) mostra a etapa 015 deslocada, e a imagem final
de 540p era mais escura e sem os degraus do canto. Eu tinha dado a imagem de 540p como certa olhando só a captura
reduzida; não estava.

**Correção** (`ds2_native_position_scale`, padrão `true`): o renderizador já tinha uma reescrita de SPIR-V da
posição do pixel para a grade de raster (`me_fragcoord_xy_spirv.h`); ela ganhou o modo `Scale`, que multiplica X e Y
por `4/q` (e por 0,5 em X junto com a grade). A escala entra na chave do pipeline (`PipelineKey::fill`, bits 16–23,
só com pixel shader e fora de 720p), no pré-aquecimento e no cache de módulos. Um shader que a reescrita não cobre
continua com pixels do host e gera aviso, sem rejeitar o desenho. No PC a 540p: 8 módulos reescritos no primeiro
mapa, nenhum aviso, etapa 015 alinhada e imagem final equivalente à de 720p.

**Desempenho a 540p no console** (mesma sessão; janelas de 10 s): de 9,6 a 17,9 quadros/s conforme a direção da
câmera, GPU de 55,8 a 104,5 ms por quadro. Olhando para a casa das firekeepers: cena 34,9 ms, alvos de 640: 13,7,
**edram_import 29,8** (contra ~4 na direção oposta), edram_alias 17,3. O relatório dessa janela:
`draw/depth-repr-switch=3919ops/297744tiles`, `whole-target runs: depth-repr-switch=844` em ~96 quadros, ou seja,
~9 conversões de profundidade de alvo inteiro por quadro por troca de representação. É o próximo alvo.

## Rodada 4: as limpezas do atlas de sombras em modo só profundidade (2026-10-10)

**Nono build no console** (`out/switch_logs/009/ds2_014.log`, 540p, sem overclock): imagem certa segundo quem jogou
(o fantasma sumiu), 8 shaders reescritos, nenhum aviso. Dois estados estáveis na primeira fogueira:

| direção | quadros/s | GPU por quadro | cena | 640 | edram_import | edram_alias |
|---|---|---|---|---|---|---|
| saída (as três fogwalls) | 14,1–14,5 | 69–71 ms | 23,0 | 5,1 | 3,3 | 17,1 |
| casa das firekeepers | 8,4 | 119,2 ms | 32,3 | 10,7 | **50,1** | 17,1 |

Na vista da casa: `draw/depth-repr-switch=7392ops/355152tiles` em 84 quadros (88 conversões de profundidade por
quadro, 13 delas de alvo inteiro).

**O que são.** Reproduzido no PC girando a câmera (`tools/live.py pad "rright ..."`) com
`ds2_native_edram4_pair_report`. Nessa vista há luzes com sombra própria: o jogo usa um atlas de profundidade em
EDRAM (base 0, passo 1040, D24FS8) e, para cada mapa de sombra (regiões de 128 ou 256 pixels), faz o Clear do
Direct3D em três partes: o miolo alinhado a blocos pela superfície 4x apelidada (passo 520), as bordas por
retângulos comuns em 1x, e só então desenha os objetos e copia a região para a textura `1326C000`. O mesmo vale
para as cascatas do sol (passo 800, D24S8), que usam os mesmos blocos antes.

O atalho das limpezas (rodada 2) não pegava esses Clears: o relatório novo `ds2_diag_rect_proof` mostrou
`estimated, overwrite not proven` para o retângulo de 3 vértices na superfície `08020208`. O jogo desenha essas
limpezas no modo de EDRAM só profundidade (`RB_MODECONTROL` 5) com o pixel shader que ficou carregado do desenho
anterior, e nessa vista ele tem `kill`; a prova de sobrescrita total exige um shader sem descarte e sem teste de
alfa. Sem a prova, o renderizador importava o atlas inteiro para a vista apelidada, desenhava a limpeza lá e
depois trazia os blocos de volta para a vista de 1 amostra aos pedaços, a cada desenho recortado de cada mapa:
no PC, 160–190 conversões e ~6.000 blocos por quadro.

**Correção** (`ds2_native_edram4_overwrite_depth_only`, padrão `true`, em `ProveFullOverwrite`): no modo 5 nenhum
pixel shader roda (o próprio renderizador desenha sem estágio de fragmento), então descarte, teste de alfa,
alpha-to-coverage e exportação de profundidade do shader carregado não entram na prova.

**No PC, mesma vista:** nenhuma conversão por troca de representação; restam ~25 importações pequenas por quadro
(32 blocos) do dono anterior de outro formato (`D000/f0:800x800 → D000/f1:1040x1040`), nos blocos parciais das
bordas (`partial tile: owner unsupported`; as bordas vêm num desenho de dois retângulos, que não vira limpeza
redirecionada). Atlas de sombras e imagem final conferidos a olho num quadro rastreado (regiões com profundidade de
cena e fundo limpo); não houve comparação A/B bit a bit.

**Décimo build no console** (`out/switch_logs/010/ds2_016.log`, 540p, sem overclock, casa das firekeepers):

| | quadros/s | GPU por quadro | edram_import | thread do anel por quadro |
|---|---|---|---|---|
| nono build | 8,4 | 119,2 ms | 50,1 ms | — |
| décimo build | 11,2–12,0 | 84,5–89,2 ms | 1,2 ms | 75,4 ms (1.169 desenhos) |

Nenhuma conversão por troca de representação (`depth-repr-switch` sumiu do relatório); restam ~48 importações
pequenas por quadro (`draw/depth-depth=5635ops/9200tiles` em 116 quadros) e as recusas `depth not proven constant`
e `partial tile: owner unsupported` (3 por quadro cada) das bordas. Dos 86 ms de GPU, 17,9 são `gap` (GPU parada):
nessa vista o limite passou a ser a **CPU**, com a thread do anel a 75 ms por quadro (64 µs por desenho). Na vista
da saída: 14,6 quadros/s, 68,1 ms de GPU, anel a 47,5 ms. As sombras ainda não foram conferidas por quem jogou.

## Queda do sistema durante uma sessão: pcv sem memória de sessões (2026-10-10)

Aos 2,5 minutos da sessão `ds2_016` o console mostrou a tela de erro do Atmosphère. Relatórios do mesmo segundo
(cópias em `out/switch_logs/010/`):

| processo | resultado | significado |
|---|---|---|
| `pcv` (010000000000001A), fatal | 2011-0102 | sf/hipc: sem memória de sessões |
| `ptm`, `Bus` | 2011-0301 | sessão fechada (consequência da queda do pcv) |
| `hoc:clk` (00FF0000636C6BFF) | 2345-0048 | o sysmodule de clocks do console (Horizon OC), logo depois |

O log do jogo vai normal até 16:55:19,7 (relógio do console); a falha é das 16:55:20. Os clocks estavam no padrão
(CPU 1020, GPU 460,8, memória 1331,2 MHz), SoC a 37 °C. Não há relatório do nosso processo.

O que se sabe: o `pcv` (o serviço de clocks e tensões) ficou sem memória para sessões. Clientes dele nesse console:
o sysmodule de clocks, a barra de status (overlay) e o nosso processo, que pelo código herdado do sa2-nx mantinha
**três sessões `clkrst` abertas** a sessão inteira (CPU, GPU e memória, lidas a cada segundo para a linha `clocks:`
do `rex_profile.log`). O que não se sabe: quem de fato esgotou as sessões. Aconteceu uma vez em ~16 sessões com o
mesmo código.

Mudança (`sdk/src/ui/switch_perf.cpp`, marcada `ds2-nx`): o processo não mantém mais nenhuma sessão `clkrst`; os
clocks são lidos no primeiro relatório e uma vez por minuto, abrindo e fechando as sessões na hora. Se a queda
voltar com esse build, o próximo passo é testar sem o overlay e sem o sysmodule de clocks para isolar.

## Décimo primeiro build no console: sombras certas, sem queda, e a CPU como limite (2026-10-10)

`out/switch_logs/011/ds2_017.log` (7 min, 540p, sem overclock): alguns minutos na fogueira, volta à casa das
firekeepers, primeira fogwall, inimigos, até o ninho de troca. Quem jogou comparou as sombras lado a lado com um
vídeo do jogo original e não achou diferença. Sem queda do sistema, nenhum desenho ou cópia rejeitado, 386 pipelines.
O save do console (`logs/ds2`) foi copiado para o PC (o anterior está em `out/pc_save_backup2`).

| trecho | quadros/s | GPU por quadro (com `gap`) | `gap` | thread do anel | desenhos |
|---|---|---|---|---|---|
| fogueira, parado | 18,6–19,2 | 52–54 ms | 5,7 | 36,5 ms | 625 |
| tutorial (fogwall, inimigos) | 12,3–16,5 | 61–81 ms | 13–34 | 32–50 ms | 600–960 |

`edram_alias` continua fixo em 17,2 ms por quadro e `edram_import` em 0,1–0,2.

**O limite agora é a CPU inteira, não uma thread.** No tutorial a GPU fica de 20 a 34 ms por quadro parada (`gap`)
com a thread do anel a 32–50 ms: nem a GPU nem o anel fecham a conta sozinhos. O `rex_profile.log` do mesmo trecho:
CPU total de 266–287 % com o processo limitado a 3 núcleos (máscara 0x7), ou seja, quase tudo ocupado:

| thread | CPU |
|---|---|
| GPU ring native (o renderizador) | 66 % |
| Main XThread (o jogo) | 62 % |
| XThread648EB540 (jogo) | 48 % |
| thread sem nome no núcleo 0, prioridade 0x2C | 20 % |
| MtTaskWorkerThread x3 (jogo) | 18 + 13 + 10 % |
| DrawCommandThread (jogo) | 15 % |
| XMA Decoder | 13 % |

As pilhas não foram amostradas nessa sessão (`switch_profile_stacks` desligado), então ainda não se sabe em que
funções esse tempo vai (código recompilado útil, esperas ativas do jogo, ou o renderizador).

**Próximos passos de CPU, na ordem:**

1. perfil com pilhas no console (só o `ds2.toml` muda; o ELF do build em uso está em `out/switch_elf/b08b0f7/`);
2. o que o perfil mostrar no renderizador (60 µs por desenho) e nas threads do jogo;
3. LTO e PGO, que o build do Switch deste projeto ainda não tem (`DS2_SWITCH_LTO_JOBS` é passado pelo script e
   ignorado pelo CMake). No sa2-nx o PGO rendeu ~15 % sobre o LTO. Custo nesta máquina: a compilação completa do
   código recompilado levou ~100 minutos; um link com LTO do programa inteiro refaz esse trabalho a cada mudança no
   renderizador, o que não cabe no ciclo de rodadas. Caminho previsto: otimizar o código recompilado uma vez (ele não
   muda entre rodadas) e manter o renderizador fora do LTO.

## Perfil de CPU com pilhas no console e a pergunta do deko3d (2026-10-10)

`out/switch_logs/012/` (sessão `ds2_018`, 540p, sem overclock, com `switch_profile_stacks`; ELF do build em
`out/switch_elf/b08b0f7/`): 1 minuto na fogueira, o resto do tutorial até a fogueira de Majula. Com a amostragem
ligada a CPU fica em 297 % de 300 % mesmo parado. Relatório: `python tools/switch_profile_report.py
out/switch_logs/012/rex_profile.log --elf out/switch_elf/b08b0f7/ds2.elf --max-fps 30` (saída em `report.txt`).

**Onde a CPU vai** (16.462 amostras em execução, 35 intervalos abaixo de 30 quadros/s):

| thread | amostras | parte |
|---|---|---|
| Main XThread (jogo) | 4.024 | 24,4 % |
| GPU ring native (o renderizador) | 3.452 | 21,0 % |
| MtTaskWorkerThread (jogo) | 2.875 | 17,5 % |
| XThread do jogo (B11F4AC0) | 2.510 | 15,2 % |
| DrawCommandThread (jogo) | 917 | 5,6 % |
| thread "?" no núcleo 0: a gravação adiada, onde rodam as chamadas Vulkan (NVK) | 603 | 3,7 % |
| XMA Decoder | 403 | 2,4 % |
| o resto (16 threads) | ~1.680 | ~10 % |

O código recompilado do jogo soma cerca de dois terços. Na thread principal o perfil é plano (a função mais quente
tem 1,5 %; pelas pilhas, `sub_82DACC38` 15 % e `sub_82DBB028` 9 % inclusivos): não há uma espera ativa dominando.

**A thread do anel** (3.452 amostras):

| função | parte |
|---|---|
| `DrawsVulkanImpl::Draw` (tempo próprio) | 36,4 % |
| `CopyVertices` | 8,1 % |
| `memmove` | 5,9 % |
| `WriteConstantRunRaw`, `armDCacheClean_L0` | 3,0 % cada |
| `Type3`, `_free_r`, fingerprints, `RexGmQueryProtect`, identidade de shaders, `PublishEDRAM4`... | 2,5 % ou menos cada |

Dentro de `Draw`, 969 das 1.257 amostras (77 %) caem em 0x90 bytes de código: o laço geral de conversão de índices
(`rev16`, soma da base, mínimo/máximo, teste do valor de reinício, um índice por vez, saída em 32 bits). O jogo
desenha quase tudo como tiras de triângulos de 16 bits com reinício de primitiva em 0xFFFF (`index layout seen:
16 bit, endian 1, restart true, type 6`), e o caminho rápido herdado (NEON, cache por quadro, envio em 16 bits)
só aceitava índices sem reinício. Isso é 28 % da thread do anel, ~6 % de toda a CPU.

**Correção** (`ds2_native_indices16_restart`, padrão `true`): 0xFFFF também é o valor de reinício do Vulkan para
índices de 16 bits, então essas tiras passam pelo caminho rápido como estão; só o maior índice é recalculado sem os
valores de reinício. No PC a geometria sai igual (captura no ninho de troca), sem erro novo de validação.

**deko3d** (documentos do nfsmw-nx, ramo `deko3d-implementation`: `docs/HowToDeko3d.md` e
`docs/deko3d-devlog.md`). O que eles relatam: no NFS MW a thread do anel gastava ~9 µs por desenho, ~3,4 µs dentro
do NVK (pipeline, descriptor sets, draw), e o deko3d tira esse custo e as paradas de criação de pipeline; chegou a
35,2 quadros/s a 960x540 contra 26-29 do NVK na mesma faixa. O custo: backend novo (~13 mil linhas lá), caminho de
shaders HLSL → SPIR-V → SPIRV-Cross → GLSL → uam → DKSH com mudanças no tradutor, nada disso roda no PC (só no
console), e na primeira comparação o deko3d precisou de quase o dobro de GPU até as economias do caminho NVK serem
portadas (35-43 ms contra 19-24).

Aqui a conta é outra:

- o driver (NVK e o código comum do Vulkan) roda na thread da gravação adiada e soma **3,7 % da CPU**; é o teto do
  que o deko3d tira diretamente, mais uma parte pequena da thread do anel (`PipelineFor` 1,0 %, enfileiramento
  ~1 %);
- a thread do anel gasta ~60 µs por desenho, seis vezes o total do NFS MW, e a maior parte é código nosso que
  independe da API (índices, vértices, constantes, EDRAM);
- dois terços da CPU são o jogo recompilado, que o deko3d não toca;
- o renderizador deste jogo depende da lógica de EDRAM (vistas apelidadas, conversões por shader de fragmento e de
  computação, ZCULL com patch próprio), toda escrita contra Vulkan e conferida no PC com lavapipe.

Conclusão registrada: deko3d não é o próximo passo para este jogo; fica como opção para quando o renderizador e o
código do jogo estiverem enxutos e o driver passar a pesar. Ordem atual: índices (feito), vértices/`memmove`,
constantes, depois PGO/LTO do código recompilado.

## Décimo segundo build no console: índices mais baratos e a primeira função não registrada (2026-10-10)

`out/switch_logs/013/ds2_019.log` (4,5 min, 540p, sem overclock): de Majula até a Forest of Fallen Giants, onde o
jogo fechou e voltou ao menu do console pouco antes da primeira fogueira. Nenhuma geometria errada relatada, nenhum
desenho rejeitado.

**Rodada 5 medida.** A thread do anel caiu de 58-60 para 39-45 µs por desenho (`ring thread:` do log); o cache de
índices passou a acertar: `ME index cache: 600000 hits, 1845 MB neither converted nor copied` na sessão.

**A queda.** `logs/rex/rex_crash.log` é um `abort()` nosso, e a última linha do log do jogo diz por quê:

    [FATAL] Call to invalid or unregistered function at guest address 0x82C883C8

É uma função do jogo que o recompilador não descobriu: `tools/find_gaps.py --check 0x82C883C8` mostra o endereço
num buraco de 48 bytes entre `sub_82C883A0` e `sub_82C883F8`. O buraco tem dois thunks de 6 instruções
(`lwz r3,0(r4); mr r4,r5; lwz r11,0(r3); lwz r10,36(r11); mtctr r10; bctr` em 0x82C883C8 e o irmão em 0x82C883E0,
slot 40 da vtable), da mesma família dos vizinhos já registrados. Ninguém os chama por `bl` nem os guarda em dados:
o jogo monta os endereços em código, em 0x82C88578-0x82C885C0, com seis `lis` seguidos e os `addi` até 12 instruções
depois. `tools/pointer_fixpoint.py` procura esse padrão, mas só olhava 8 instruções depois do `lis`: pegou os dois
vizinhos e perdeu estes.

**Correção.** A janela passou a 64 instruções (`BUILT_WINDOW`; a sequência continua terminando num desvio ou quando
o registrador é reescrito). A varredura refeita achou 20 candidatos e aceitou exatamente esses dois (os outros 18
são os mesmos rejeitados de antes: fragmentos que leem registrador indefinido, desvios para dentro de outra função,
sem código ou sem terminador). Entraram em `app/overrides.toml`; o código regenerado mudou só em `ds2_funcs.h`,
`ds2_init.cpp`, `ds2_register.cpp` e nos dois arquivos que receberam as funções (101 e 236), então a recompilação
foi de minutos.

Pegadinha desta máquina: `tools/codegen.ps1` chama `python`, que aqui é o atalho da loja do Windows, e o passo
`tools/post_codegen.py` falha; rodá-lo à mão com o Python real depois de cada codegen.

**O que ainda pode haver do mesmo tipo.** O relatório de buracos lista 902 trechos de 8 bytes ou mais sem função
(343 de 8, 118 de 12, 214 de 16-63, 227 de 64-1023). A maioria é preenchimento, dados no meio do código ou funclets
de exceção; os que forem funções chamadas só por ponteiro montado de um jeito que a varredura não vê vão aparecer
como esta queda, um por vez. A varredura estrutural (`tools/gap_fixpoint.py`, que declara todo buraco que seja uma
folha completa, com ou sem referência) ainda não foi rodada neste jogo; ela pode mexer em muitos arquivos gerados,
então fica para junto da recompilação completa do PGO/LTO.

## Varredura estrutural de funções sem dono e a preparação do PGO/LTO (2026-10-10/11)

O décimo terceiro build passou do ponto da queda (`out/switch_logs/014/ds2_020.log`, sem `FATAL`).

**Varredura estrutural** (`tools/gap_fixpoint.py`, portada para o Windows: acha o `rexglue.exe`, monta o manifesto
da passagem com caminhos relativos, chama `tools/codegen.ps1` e depois `tools/post_codegen.py` com o próprio
interpretador; `--dry-run` para só contar). Ela declara como função todo buraco de 8 a 1.024 bytes cujo código
gerado é uma folha completa (cabe no buraco, termina em `blr`/`bctr` ou desvio de cauda, não usa pilha nem LR, não
lê registrador indefinido na entrada), com ou sem referência conhecida.

Dois tipos de buraco falso apareceram e passaram a ser descartados:

- `tools/find_gaps.py` conta uma instrução por linha de comentário; uma função com tabela de saltos dentro do código
  (as palavras da tabela não têm comentário) parece mais curta do que é, e os últimos bytes dela, o código dos casos,
  aparecem como buraco. Declarar uma função ali cortaria o switch dos seus casos. Critério: o buraco contém um rótulo
  `loc_` do código gerado. 33 candidatos descartados (exemplo: `sub_8328FA30`, tabela de 17 entradas em 0x8328FA58);
- tabela de endereços lida como instruções: metade ou mais das palavras são endereços alinhados dentro do código.

Resultado: passagem 1, 901 buracos novos, 365 aceitos; passagem 2, 4 novos, 3 aceitos; passagem 3, ponto fixo.
**368 funções** novas em `app/overrides.toml` (710 entradas no total): 226 são um `blr` sozinho (funções vazias),
111 terminam em `bctr` (encaminhamento por vtable, como as duas da queda), o resto são getters, setters e thunks.
`tools/switch_check.py` depois da regeneração: 0 tabelas truncadas, 0 alvos errados, 0 perdidas. 220 arquivos
gerados mudaram (cerca de 110 unidades de compilação).

Limite conhecido: a varredura só aceita folhas. Uma função sem dono que use pilha, ou cujo endereço seja calculado
de um jeito que nenhuma das varreduras vê, ainda apareceria como `Call to invalid or unregistered function`.

**PGO/LTO, o que foi montado** (de `sa2-nx`):

- `app/src/ds2_pgo.cpp`: no build instrumentado grava as contagens em `pgo/ds2_pgo_<N>.stream` ao lado do NRO, a
  cada 2 minutos, ao perder o foco (menu HOME) e ao sair; `Ds2App` chama `Start`/`DumpNow`/`Stop`;
- `app/CMakeLists.txt`: `DS2_SWITCH_LTO` (desligado por padrão), `DS2_SWITCH_LTO_JOBS`, `DS2_SWITCH_IPA_PTA`,
  `DS2_PGO` (`generate`/`use`), `DS2_PGO_DIR`. Antes o script passava `DS2_SWITCH_LTO_JOBS` e o CMake ignorava;
- `tools/win/build_switch.ps1`: `-BuildDir` e `-Lto`. Cada tipo de build tem a sua pasta, porque trocar as flags de
  uma pasta recompila o jogo inteiro: `out\nx` (o do dia a dia), `out\nx_pgo` (instrumentado), `out\nx_rel`
  (otimizado). O perfil casa entre pastas por causa do `-fprofile-prefix-path`;
- `tools/win/pgo_merge.ps1`: acha o devkitPro como o script de build e aceita `-BuildDir` e `-Python`.

Sobre o LTO neste jogo: no codegen clássico as chamadas entre funções do jogo passam por símbolos fracos
(`sub_X` é um apelido fraco de `__imp__sub_X`), então o ganho de inlining entre arquivos pode ser pequeno; no sa2-nx
ele vem junto do codegen "optimised" (chamadas diretas), que aqui ainda não foi feito. Fica medido, não presumido.

**Builds longos nesta máquina** (i5-3210M, 2 núcleos / 4 threads, `-j4`; `out/build_switch_all.log`):

| build | pasta | tempo | NRO |
|---|---|---|---|
| do dia a dia depois da varredura (210 passos) | `out\nx` | 39,7 min | 105,6 MB |
| instrumentado (`-Pgo generate`), do zero, com o SDK | `out\nx_pgo` | 115,8 min | 132,1 MB |
| LTO sem PGO, do zero | `out\nx_rel` | 122,5 min (compilar ~48, link ~74 com 2 LTRANS) | 117,8 MB |

O link do LTO usa 512 partições e ~120 MB por `lto1`; cabe com 4 em paralelo. `tools/check_switch_build.py` passa
nos três (no instrumentado apareceram dois laços sem fim do próprio jogo que o build normal compila de outro jeito:
`sub_8285E1D0` e `sub_82B482D8`, anotados em `KNOWN_HALTS`).

**Décimo quarto build no console** (`out/switch_logs/015/`): `ds2_021` com o `ds2.nro` (7,5 min, Majula e floresta)
e `ds2_022` com o `ds2-pgo.nro` (6,5 min). Nenhum `FATAL`, nenhum desenho rejeitado. No `ds2.nro`: média de 11,7
quadros/s em 39 janelas, com 42,7 ms de GPU parada por quadro (`gap`) e CPU total de 279-280 %: o jogo está preso
na CPU. O perfil: `ds2_pgo_1.stream`, 6,4 MB → 354 `.gcda` em `pgo\` (fora do git; `sum_max` 1.629.423.073).

A "aura branca" da foto enviada é a mira de trava do jogo (o ponto luminoso sobre o inimigo travado).

**Troca de resolução ao vivo** (`ds2_render_height_combo`, ligado): no Switch, L3 + R3 por um segundo alterna a
altura de render do modo em uso entre 540 e 720 (`ds2::render::ToggleHeight`; o pad é lido num `PadState` próprio em
`OnPresented`). O renderizador já refazia as imagens quando a escala muda (dock/undock do sa2-nx); aqui nunca tinha
sido exercitado. No PC (`ds2_render_test_height_at`): 540 → 720 nos menus e 720 → 540 no meio da floresta, imagem
certa nas duas (`out/pc/captures/t720.png`, `t540.png`), sem erro novo de validação.

**Build PGO + LTO** (`out
x_rel`, `-Lto -Pgo use -LtoJobs 4`, com o perfil de uma sessão de 6,5 min): 84,6 min
(o SDK já estava compilado; o link com 4 LTRANS é bem mais curto que com 2), NRO de 121,3 MB, `.text` 119,2 MB contra
115,7 do LTO sozinho e 103,5 do build normal. Conferido que o perfil é lido: o comando de `ds2_recomp.7.cpp` refeito
com `-Wmissing-profile -Wcoverage-mismatch` não avisa nada. `tools/check_switch_build.py` passa (3 laços sem fim
conhecidos, nenhum fmla vetorial; os fmadd escalares sobem de 10.499 para 13.650, cópias dos `std::fma` do próprio
jogo por inlining). ELF em `out/switch_elf/pgo_lto_1/`; o do LTO sozinho em `out/switch_elf/lto_only/`. Nenhum dos
dois foi rodado no console ainda.

## PGO + LTO medido no console, e 540p contra 720p (2026-10-11)

`out/switch_logs/016/`: `ds2_025` com o `ds2-opt.nro` (20 min, save novo) e `ds2_026` com o `ds2.nro` (8 min), sem
overclock, sem queda e sem anomalia relatada. Em três pontos parados (o matagal do início, dentro da casa das
firekeepers, a vista de Majula ao sair de Things Betwixt) quem jogou ficou ~30 s a 540p, trocou com L3 + R3 e ficou
~50 s a 720p. A troca ao vivo funcionou no console (6 trocas por sessão no log). Janelas de 10 s inteiras dentro de
cada trecho:

| ponto | resolução | `ds2.nro` | `ds2-opt.nro` | ganho |
|---|---|---|---|---|
| 1 matagal | 540p | 15,5 | 16,2 | +5 % |
| 1 matagal | 720p | 11,8 | 11,8 | 0 |
| 2 casa | 540p | 15,8 | 19,0 | +20 % |
| 2 casa | 720p | 12,2 | 13,0 | +7 % |
| 3 vista de Majula | 540p | 15,3 | 17,1 | +12 % |
| 3 vista de Majula | 720p | 12,5 | 12,7 | +2 % |

(quadros por segundo). Onde a thread do anel não está esperando a GPU, o custo por desenho caiu de 44,8-48,6 para
41,7-41,8 µs.

**O que limita em cada caso.** A 720p a GPU não tem folga (2-3 ms parada por quadro) nos dois builds: o ganho de CPU
não aparece. A 540p o `ds2-opt.nro` no ponto 1 também encosta na GPU (1,8 ms parada; a cena custa 32 ms ali, com o
mato). Nos pontos 2 e 3 o build normal deixava a GPU 7,5 e 11,6 ms parada, e o otimizado aproveita.

**540p contra 720p** (mesmo build, mesmo ponto): de +31 % a +46 % de quadros por segundo a 540p. Por categoria de GPU,
720p → 540p: `edram_alias` 24,6-30,4 → 14,1-17,4 ms, cena 30-37 → 24-32 ms.

**Próximo alvo de GPU: `edram_alias`.** São 14-17 ms por quadro a 540p (um quarto do quadro) e 25-30 a 720p, fixos em
todos os pontos. Pelos tempos por passe da rodada 4, a categoria é dominada por passes cujo primeiro desenho usa
`VS n3` com `PS n720` e `PS n184` na superfície 4x apelidada do alvo principal: desenhos de tela cheia do jogo feitos
por amostra (não são Clears), seguidos da conversão do alvo inteiro para a vista de 1 amostra. Ainda não investigado.

## Mesa 26.2.4 (2026-10-11)

Quem joga deixou na raiz do projeto `mesa-26.2.4-switch-unified-horizon-sdk.zip` (um SDK já compilado, mesmo layout
do nosso: `opt/devkitpro/portlibs/switch`). Extraído em `_tools/mesa-26.2.4-sdk/`. Comparando as cadeias de texto das
duas `libvulkan.a`:

| | atual (`_tools/mesa-switch`) | zip |
|---|---|---|
| versão | Mesa 26.2.1 (git-1a8c1a66d6) + patch do nfsmw-nx | Mesa 26.2.4 (git-606441c90e) |
| ZCULL (plano por imagem de profundidade) | sim (`ZCULL: plano de ...`) | não (só os nomes dos registradores) |
| `NVK_SWITCH_DIBUJO` (caminho de desenho mais barato, medição por desenho) | sim | não |
| `NVK_SWITCH_DYN_UBO_DELTA` (set 4 por diferenças) | sim | não |
| `NVK_COPY_ENGINE`, `NVK_SUBTILING_KNOB`, `NVK_SHADER_STATS` | sim | não |
| `NVK_SWITCH_CPU_WRITE_MEM_UNCACHED`, `_MAPPED_COMPLETION`, `_NO_UBO_CBUF`, `_PERF_LOG` | sim | sim |

Ou seja, é o mesa-switch mais novo sem as mudanças de driver do StevensND, que são as que este renderizador (via
sa2-nx) usa; o `mesa/` do nfsmw-nx continua em 26.2.1. O ZCULL sozinho valeu ~8 % de GPU aqui (137,9 → 127,2 ms).
**O link contra o SDK do zip falha** (`out/nx_build_mesa264.log`): milhares de referências indefinidas a funções da
biblioteca base do Rust (`core::panicking::panic`, `panic_fmt`, `panic_bounds_check`, `core::fmt::write`,
`alloc::alloc::handle_alloc_error`...), vindas do `nil` e do `nak_rs`. A `libvulkan.a` do zip só traz as instâncias
genéricas do `core` (crate `CsR2NHd66MWo`), não a biblioteca em si; a nossa (crate `CslpfLnFkgLmC`, de outro
compilador Rust) traz. Sem a `libcore` do mesmo `rustc` que gerou o zip não há como ligar, e a deste PC é de outra
versão. O build do dia a dia voltou para o SDK atual (`-Reconfigure` com o `NVK_SDK` antigo; só religa).

Para ter o 26.2.4 de verdade o caminho é compilar aqui o mesa-switch no commit `606441c90e` com o patch do nfsmw-nx
portado (ele é contra o `1a8c1a66d6f`, 35 arquivos). Não foi feito: não há medida de que o 26.2.4 traga ganho para
este jogo, e sem o patch ele perde o ZCULL.

## UnleashedRecomp-NX e MarathonRecomp-NX: o que serve para este porte (2026-10-11)

Clonados em `_ref/` (fora do git). Os documentos lidos: `UnleashedRecomp-NX/docs/SWITCH-PERFORMANCE.md` (10 rodadas,
com o resultado de cada conjunto de teste) e `MarathonRecomp-NX/docs/SWITCH-OPTIMISATIONS.md` (o catálogo completo,
com o porquê de cada mudança ser exata). São portes do XenonRecomp com renderizador próprio (plume), então a parte de
renderizador não se transplanta; a parte de **geração de código do jogo** e de **runtime** sim, e é onde está a nossa
CPU (dois terços no código recompilado).

**O ponto central.** O gerador emite cada função do jogo como um apelido fraco e `noinline` (para os hooks poderem
substituí-la), e toda chamada passa por esse apelido: o GCC não consegue embutir nenhuma, com ou sem LTO. O passo
"direct calls" deles faz as chamadas entre funções sem hook irem direto ao `__imp__sub_X` (Unleashed: 365.513
chamadas para 41.315 funções). É o mesmo passo que o sa2-nx tem em `tools/direct_calls.py`, dentro do codegen
"optimised", que este porte ainda não usa (estamos no "classic"). Isso explica o ganho modesto do nosso PGO + LTO.

**Já temos, igual a eles:** LTO, PGO (coleta em seção, `-fprofile-update=single`, `-fprofile-partial-training`),
`-fipa-pta`, alvos de profundidade sem `TRANSFER_DST` para o ZCULL, cache de pipelines, thread de render em
prioridade não fatiada, constantes por UBO, desenhos só de profundidade sem estágio de fragmento, relógio da GPU
corrigido (1,627), perfis de CPU/GPU.

**Entra neste build (não invalida o perfil coletado):** `-fno-math-errno -fno-trapping-math` no código recompilado
dos builds otimizados (`DS2_SWITCH_MATH_FLAGS`; 464 `sqrt()` no código gerado, cada um com teste e desvio para a
libm). O `-fomit-frame-pointer` deles ficou de fora por ora: o `rex_crash.log` e o amostrador de pilhas andam pela
cadeia de quadros através do código do jogo, e ainda estamos caçando erros.

**Próxima etapa de CPU, na ordem do que eles mediram** (tudo isso muda o código gerado, então pede recompilação
completa e **nova coleta de PGO**; o sa2-nx já traz as opções do gerador e as ferramentas de auditoria):

| mudança | deles | aqui |
|---|---|---|
| chamadas diretas | Unleashed R1-3; Marathon 151.618 chamadas | `tools/direct_calls.py` (lista de hooks do DS2 a conferir) |
| memória do jogo sem `volatile` + barreiras de laço | Unleashed R8; masseffect-nx +15-20 % | `pch_no_volatile.py` + `loop_barriers.py` (obrigatórios juntos) |
| CR, CTR, XER e reserva como locais; sem MSR | os dois | `cr_as_local`, `ctr_as_local`, `xer_as_local`, `reserved_as_local`, `skip_msr` |
| registradores não voláteis como locais, sem as chamadas `__savegprlr`/`__restgprlr` | masseffect-nx -10 % na thread principal | `non_volatile_as_local`, `elide_lr_stores` (aqui os helpers são 2-3 % das amostras) |
| movimentos de ponto flutuante sem trocar o modo do FPCR | sa2-nx, Marathon | `fp_moves_modeless` |
| registradores de rascunho como locais | sa2-nx | `non_argument_as_local` + lista por jogo (`tools/non_argument_audit.py`) |
| atômicos do jogo sem barreira completa | os dois (`SwitchRelaxedAtomics`) | a verificar no ReXGlue |
| seções críticas e eventos sem chamada de sistema quando ninguém espera; giro curto antes de dormir | os dois (ligados no Marathon depois de teste de áudio) | no kernel do SDK; mexe com áudio, A/B obrigatório |
| threads do motor fora do núcleo 0 | Marathon (o som, não: deu falhas de áudio) | A/B |
| funções nativas no lugar das mais quentes (RTTI, `dynamic_cast`, CRT, matrizes) | os dois | depende do perfil por função do DS2 |

Requisitos por jogo antes de ligar o codegen "optimised" (pelo `perf_overrides.toml` do sa2-nx):
`tools/read_before_write.py` e `tools/non_argument_audit.py` no código gerado, os chamadores de setjmp/longjmp, os
funclets de exceção, e a conferência no PC (modo verify e captura em lockstep contra o clássico).

**Na GPU**, o que eles fizeram no tradutor de shaders e que o nosso (mesma origem, XenosRecomp) ainda não tem: saída
antecipada do teste de alfa (os pixels descartados pulam a aritmética entre a saída de alfa e o teste; 646 de 1.096
shaders no Unleashed) e o "afundamento" das leituras de sombra para depois do teste; `gather` nos filtros de sombra;
saídas de vértice que o pixel shader não lê não calculadas. Aqui a cena custa 24-32 ms a 540p e o matagal do início
é o pior ponto, então a saída antecipada do teste de alfa é a primeira candidata. Exige regerar a biblioteca de
shaders e conferir a imagem.

**`edram_alias`, o que é.** A contagem do log (`color-resolve/color->color`, corridas de alvo inteiro) e o relatório
de pares do PC apontam para conversões de formato do alvo inteiro: o jogo desenha a cena num alvo 7e3 (formato 3,
RGBA16F no host) e o copia declarando a origem como formato 2 (2_10_10_10), isto é, lê os bits crus do HDR; o
renderizador converte o alvo inteiro de float para os bits 7e3 empacotados a cada cópia dessas, e de volta quando o
jogo volta a desenhar nele (`C000/f3:1280x1280 -> C000/f2` e o inverso, 720 blocos cada). São 4-6 por quadro. A
investigar: se a textura resultante é lida como 7e3 de novo, as duas conversões se anulam e a imagem float pode ser
entregue direto.

## Rodada 6: perfil de PGO completo e conversões de formato como passe de fragmento (2026-10-11)

**Perfil.** `out/switch_logs/017/pgo/`: `ds2_pgo_1.stream` (6,5 min) e `ds2_pgo_2.stream` (27 min, do início até o
Last Giant, sessão `ds2_027`, sem `FATAL`). Juntados do zero em `pgo\` (o da sessão 1 está em `out/pgo_session1`):
354 `.gcda`, `runs = 2`, maior contagem de arco 8.220.895.832. Build `out\nx_rel` (`-Lto -Pgo use`, mais
`-fno-math-errno -fno-trapping-math`): 62,9 min depois de retomado (o notebook travou no meio do primeiro: eu
rodava uma sessão do jogo no PC junto com a compilação; uma coisa de cada vez daqui em diante). NRO de 120,8 MB,
`tools/check_switch_build.py` passa. ELF em `out/switch_elf/pgo_lto_2/`.

**`edram_alias`, o que era de fato.** A lista "GPU time by pass" mostrava `[cat 12 VS3 PS720]` e `[cat 12 VS3
PS184]`: não são shaders do jogo, são as conversões (o "PS" ali é o número de blocos: 720 = o alvo de 1280x720, 184 =
o de 640x360). Por quadro, no console: 1 conversão do alvo inteiro (720 blocos) em cópia de cor e 4-5 pequenas em
desenhos; no PC, o relatório de pares mostra `C000/f3 -> C000/f2` (o alvo 7e3 lido como 10 bits, os bits crus do
HDR) duas vezes por quadro e `C000/f0 -> C000/f3` uma. Todas rodavam como **compute shader**: o sa2-nx tem o mesmo
trabalho como passe de fragmento (`ds2_native_conversion_frag`, "same words bit for bit", sem tráfego de imagem de
armazenamento nem troca de motor 3D <-> compute), desligado por padrão.

Ligado no PC (540p, Majula, com o save do console): `color conversions as fragment passes: 6 done, 0 declined` por
janela, nenhum erro novo de validação, imagem certa (`out/pc/captures/frag1.png`) e a cadeia HDR com valores
coerentes no despejo do quadro (`12366000`, `117AC000`). Vai ligado no `ds2.toml` do pacote;
`configs/ds2_teste_conversao_compute.toml` volta ao caminho antigo. O ganho só o console mede (a categoria
`edram_alias` do log).

**Miniaturas erradas na lista de saves (em aberto).** Foto do console: dos três retratos da tela "Data List", o do
meio sai certo e os outros dois são ruído. Os arquivos de slot (`USER_DATA01`..`0a`, 111.308 bytes) não guardam
imagem: os dos dois primeiros saves têm a mesma estrutura (dados, zeros, dados) e o terceiro tem conteúdo de alta
entropia com ponteiros e floats, o que precisa ser conferido (o jogo carrega os três normalmente). No log do console,
ao abrir o menu o jogo cria quatro texturas de cópia de 1280x720 (`158B4000`, `15C4C000`, `15FE4000`, `1551C000`):
candidatas a retrato. No PC a lista aparece por poucos segundos antes de o jogo carregar o save e os retratos não
chegam a ser desenhados nesse tempo, então não foi reproduzido. Próximo passo barato: abrir a lista no console com
`configs/ds2_melhorias_desligadas.toml`; se os retratos saírem certos, é um dos atalhos de cópia herdados.

## Rodada 6 medida no console; rodada 7: retratos dos saves e a conversão texel a texel (2026-10-11)

**Décimo sexto build no console** (`out/switch_logs/019/ds2_029.log`, `ds2-opt.nro` com o perfil das duas coletas e
`ds2_native_conversion_frag`; nenhum `FATAL`; `302 done, 0 declined` por janela; 674 desenhos rejeitados, todos na
lista de saves, o que só foi visto na rodada seguinte: ver "Rodada 7 medida"). Nos três
pontos, contra a sessão `ds2_025` (build otimizado anterior, conversões por compute):

| ponto | resolução | quadros/s antes → agora | `edram_alias` antes → agora | anel µs/desenho |
|---|---|---|---|---|
| 1 matagal | 540p | 16,2 → 16,5 | 14,1 → 10,7 ms | 37,3 |
| 1 matagal | 720p | 11,8 → 12,6 | 24,6 → 18,6 ms | |
| 2 casa (outra posição: 1.209 desenhos contra 824) | 540p | 19,0 → 15,1 | 14,4 → 10,9 ms | 38,5 |
| 2 casa | 720p | 13,0 → 11,8 | 25,1 → 18,8 ms | |
| 3 vista de Majula | 540p | 17,1 → 16,9 | 17,4 → 13,2 ms | 38,8 |
| 3 vista de Majula | 720p | 12,7 → 13,7 | 30,4 → 22,9 ms | |

O ponto 2 não é comparável (a câmera estava noutro lugar). A conversão por fragmento tira 3-4 ms a 540p e 6-7,5 ms a
720p; o perfil maior leva a thread do anel de ~41,7 para ~38 µs por desenho. O que sobra em `edram_alias` é quase
todo uma conversão por quadro do alvo inteiro (`[cat 12 VS19 PS720]`, ~5 ms por quadro a 540p): o shader
`me_edram_color_to_color.frag` calcula o bloco de cada pixel com cinco divisões inteiras por valores que não são
constantes, e a GPU não tem instrução para isso.

**Conversão texel a texel** (`ds2_native_conversion_frag_identity`, padrão `true`): quando as duas vistas têm o
mesmo passo em blocos, o mesmo arranjo de amostras e o mesmo primeiro bloco, e a corrida é de fileiras inteiras (o
recorte já é exatamente a corrida), cada texel de destino lê a própria posição; o shader pula a aritmética de blocos
(campo `source_64bpp = 2` das constantes). No PC, em Majula: `12 done (12 texel for texel)` por janela, imagem e
cadeia HDR certas.

Na casa das firekeepers voltou a aparecer `edram_import` (6,6 ms por quadro, `draw/depth-depth=10827ops` em 152
quadros): são as bordas das limpezas do atlas de sombras com bloco parcial de dono de outro formato, o resto
conhecido da rodada 4. Fica para depois.

**Retratos da lista de saves.** Fotos do console com quatro saves: com o `ds2.toml` normal só o retrato do save
recém-jogado sai certo, os outros ficam vazios e um é ruído; com `ds2_melhorias_desligadas` nenhum sai certo. O log
(`ds2_030`, `ds2_031`) mostra o que o jogo faz ao abrir a lista: para cada save, ~33 desenhos num alvo copiado
para uma textura de 256x152 e mais um desenho copiado para uma de **128x76**; essas de 128x76 aparecem no inventário
como `0 reads *** NOBODY READS IT ***`, isto é, nenhum desenho as amostra como textura copiada: o jogo lê os
pixels pela CPU (para a interface). O renderizador só escreve de volta na memória do jogo texturas copiadas de até
4.096 texels (`ds2_native_resolved_read_texels`, as de exposição), e 128x76 são 9.728: o jogo lia o que houvesse
na memória (zeros: retrato vazio; lixo: ruído).

Correção (`ds2_native_resolved_read_rare_texels`, padrão 65.536): uma textura copiada maior que o limite normal e
de até esse tamanho também é escrita na memória do jogo quando a cópia é rara (a primeira, ou nenhuma nos últimos 30
quadros), e nesse caso o trabalho é enviado e esperado ali mesmo, para os texels estarem na memória antes de o anel
seguir (no console de origem um lock de textura espera a GPU). Os alvos do pós-processamento, copiados todo quadro,
só passam pela regra na primeira cópia (no PC: 12 leituras ao entrar no 3D, nenhuma depois). A lista de saves não
foi reproduzida no PC ("Continue" carrega direto o save mais recente), então a confirmação é do console.

## Rodada 7 medida no console; o desenho rejeitado da lista de saves (2026-10-11)

**Décimo sétimo build no console** (`out/switch_logs/020/ds2_032.log`, `ds2-opt.nro`, 17:28-17:39, seis trocas de
resolução, nenhum `FATAL`; os quatro retratos aparecem na lista de saves, foto do console). Nos três pontos, contra a
sessão `ds2_029` (décimo sexto build):

| ponto | resolução | quadros/s antes → agora | `edram_alias` antes → agora | GPU ociosa por quadro |
|---|---|---|---|---|
| 1 matagal | 540p | 16,5 → 16,6 | 10,7 → 3,1 ms | 11,7 ms |
| 1 matagal | 720p | 12,6 → 15,4 | 18,6 → 5,1 ms | |
| 2 casa | 540p | 15,1 → 17,0 | 10,9 → 3,5 ms | 9,5 ms |
| 2 casa | 720p | 11,8 → 15,4 | 18,8 → 5,6 ms | |
| 3 vista de Majula | 540p | 16,9 → 18,1 | 13,2 → 3,5 ms | 16,9 ms |
| 3 vista de Majula | 720p | 13,7 → 17,2 | 22,9 → 5,9 ms | |

A conversão texel a texel tirou 7-10 ms de GPU por quadro a 540p e 13-17 ms a 720p. A 720p isso virou 2,8 a 3,6
quadros/s; a 540p quase nada, porque ali a GPU já fica parada 9 a 17 ms por quadro esperando a CPU (anel: 36,6 a
42,3 µs por desenho). O limite a 540p voltou a ser a CPU; o código gerado é o próximo passo.

**5.873 desenhos rejeitados nessa sessão** (`resolved logical crop rejected 14AE9000 128x76: unproven logical fetch
layout`, causa 109), um por quadro enquanto a lista de saves fica aberta. Não é novo: as sessões `ds2_028` a
`ds2_031` têm o mesmo (1.175, 674, 962 e 609), e eu tinha escrito "nenhum desenho rejeitado" olhando só as janelas do
jogo. O que acontece: o jogo copia o retrato de cada save para uma textura de 256x152, reduz para 128x76, lê os
pixels pela CPU, libera as duas e cria a textura que a interface desenha (128x76, preenchida pela CPU). A do
primeiro save cai no endereço `14AE9000`, onde tinha estado a cópia de 256x152; o renderizador ainda tinha a entrada
da cópia naquele endereço, viu um tamanho que não é um recorte dela (passo de linha 128 contra 256) e descartou o
desenho. É o terceiro caso de memória reaproveitada (os outros: busca linear, busca comprimida).

Correção (`ds2_native_other_pitch_fetch_not_resolved`, padrão `true`): quando todas as outras condições do contrato
valem (um nível, 2D, em blocos, na base da cópia, formato compatível) e só o passo de linha difere, as linhas da
busca não podem ser as que a cópia escreveu: a busca é lida da memória do jogo, como textura comum. Uma linha de log
por endereço (`another row pitch, so the memory was reused`).

**A lista de saves no PC.** O menu do título é `New Game / Continue / Information / Go Online`; "Continue" abre a
lista. O roteiro de botões do `guided_session.ps1` aperta START três vezes e A a cada 6 s, e os tempos das telas
variam de uma execução para outra, por isso a lista "carregava direto": era o roteiro confirmando. Com
`-Pad "16-16.6:a,30-30.6:start,42-42.6:start"` e `-Extra "REX_DS2_DIAG_DRAW_EVERY=1"`, e daí à mão com
`tools/live.py` (START no título, A nos avisos "No Ethernet cable" e "Starting in offline mode", A em "Continue"), a
lista abre e fica. No lavapipe cada retrato leva ~40 s (um único passe de 37 s). Resultado
(`out/pc/captures/x6.png`): os quatro retratos, 8 leituras raras, a linha do passo de linha diferente para
`14AE9000`, nenhum desenho rejeitado.

**Folga da regra das cópias raras** (`ds2_native_resolved_read_rare_frames`, padrão 8; era 30 fixo). O jogo desenha
os retratos com 23 a 29 quadros de intervalo e reaproveita os endereços dos já liberados (`14AE9000`, `15436000`,
`14AF5000`, `15436000`: no console e no PC o reaproveitamento foi a dois retratos de distância, 58 quadros). Com o
limite em 30, um reaproveitamento a um retrato de distância ficaria de fora e o retrato sairia vazio.

## Código gerado otimizado (2026-10-11 e 12)

Etapa 2 da ordem combinada (custo fixo da GPU, código gerado, corte por alfa nos shaders). O código gerado "clássico"
guarda todo o estado do PowerPC na estrutura `ctx`, na memória; como qualquer escrita na memória do jogo pode, para o
compilador, ser uma escrita em `ctx`, quase todo uso de registrador vira uma leitura ou escrita de memória. O sabor
"otimizado" do sa2-nx muda como o C++ é escrito, sem mudar o que o jogo calcula. O arquivo novo
`app/perf_overrides.toml` liga, para o DS2:

- `cr_as_local`, `ctr_as_local`, `xer_as_local`, `reserved_as_local`: cr0-cr7, ctr, xer e a reserva do `lwarx` como
  variáveis locais;
- `skip_msr`, `dform_disp_split`, `fctiwz_inline`;
- `non_volatile_as_local`: r14-r31, f14-f31, v14-v31 e v64-v127 como locais (somem as chamadas de
  `__savegprlr`/`__restgprlr` e o tráfego de pilha desses registradores);
- `elide_lr_stores` e `fp_moves_modeless`;
- depois do gerador (`tools/post_codegen.py`): memória do jogo sem `volatile` (16 macros do `ds2_pch.h`), barreiras em
  51.794 cabeças de laço (obrigatórias com a memória sem `volatile`) e **217.593 chamadas diretas** entre funções sem
  hook (`tools/direct_calls.py`), que o GCC passa a poder embutir.

Um codegen leva 10 min aqui, mais 3 min dos passos seguintes. `tools/codegen.ps1` agora acha o Python de verdade
sozinho (o `python` do PATH desta máquina é o atalho da Microsoft Store).

**`non_argument_as_local` ficou de fora.** `tools/non_argument_audit.py` no código clássico do DS2: 340 funções
recebem um valor em r0/r11/r12/f0/v32-v63 na entrada (250 em r12; cadeias de funções que passam v33-v63 e f0 adiante),
fora os auxiliares de salvar/restaurar, e 329 leituras depois de uma chamada de um valor que a função chamada pode
ter escrito (277 em r11, depois de `sub_82FB2578`, `sub_82FB2540` e `sub_8266B030`). No SA2 eram meia dúzia. Precisa
de um trabalho por função antes; fica para uma rodada própria.

**setjmp / longjmp.** O DS2 tem o par do CRT: `sub_82FB8A30` (a única função do programa com `mfcr`; 68 chamadas) e
`sub_82FB78D0` (433 chamadas: o caminho de erro de alguma biblioteca). Traduzido como chamada comum, um longjmp
restaura r1 do `jmp_buf` e depois volta para quem o chamou, com a pilha de outra função. Com `setjmp_address` e
`longjmp_address` no `overrides.toml` o gerador emite setjmp/longjmp do host, indexados pelo endereço do `jmp_buf`.
O perfil de PGO dos 33 minutos de jogo (compilei de novo `ds2_recomp.46.cpp` e o `.20` com
`-fdump-ipa-profile-blocks` e o perfil das duas coletas) tem o setjmp com 3.920 entradas e todos os arcos do longjmp
em zero: nenhum longjmp rodou no console até aqui. Com os registradores em locais, depois de um longjmp os r14-r31 de
quem chamou o setjmp ficam com o valor da hora da falha, não o da hora do setjmp (mesmo risco aceito no SA2).

**Leituras antes de escrita** (`tools/read_before_write.py`, 4,6 min): uma função que lê um registrador não volátil
antes de escrevê-lo esperava o valor de outra função, e com locais lê zero.

- Sem sementes além do `throw` do C++ (`sub_82FBF820`, 35 chamadas, chama `RaiseException`): 70 funções. 43 delas
  eram caminhos depois de uma chamada a `sub_8266B030` ou `sub_82AB5F50`, dois impressores de "erro fatal" com
  argumentos variáveis: 23 funções *terminam* com `bl` para um deles, sem nada depois, então o compilador original
  sabia que não voltam. Com as três como sementes: **27**.
- 8 são reais, e mais duas que a ferramenta lista como "só escrita":
  - **o decodificador de WMV** (`movie/prologue.wmv`, o vídeo de abertura) guarda constantes em v64-v127 através de
    funções C: `sub_8338E090` grava v64-v127 num buffer e carrega os 64 de outro, `sub_8338DD48` faz o mesmo e
    recarrega v64-v72, e sete folhas escritas à mão (`sub_831452E0`, `sub_83145918`, `sub_833A91A0`, `sub_833A9548`,
    `sub_833A9808`, `sub_833A9A70`, `sub_833AAB98`) leem v64, v65, v69 ou v72 sem nunca escrevê-los. Com locais as
    duas primeiras carregariam variáveis que somem no `return` e as folhas leriam zero. As nove ficam
    `share_registers` (usam `ctx`); nenhuma chama auxiliar de salvar/restaurar nem escreve r14-r31, e nenhuma das
    funções que as chamam entrega registrador vetorial (conferido no código gerado), então `ctx.v64`-`v127` guardam as
    constantes do decodificador como os registradores de verdade guardavam;
  - `sub_829A3604`: dez instruções no fim da função de heap `sub_829A2DF0`, alcançadas por `beq`/`bne` dela (e por
    um ponteiro, da varredura antiga); lê o r31 da dona. `share_registers`.
- As outras 19 ficam como estão: funções normais, com prólogo que salva o registrador antes de usá-lo, em que algum
  caminho lê uma variável que o código original também não tinha inicializado (`sub_8235E600`, `sub_823C2660`,
  `sub_824CB840`, `sub_8259D220`, `sub_8259DA80`, `sub_82B63548`, `sub_82C02E10`, `sub_82C9F088`, `sub_82EF1290`,
  `sub_82F73DF0`, `sub_82F9B720`); idiomas que não dependem do valor (`xor r30,r30,r30` em `sub_829982F0` e
  `sub_82998680`, `subfe rX,rY,rY` em `sub_83009D40` e `sub_830B12C8`); o setjmp do jogo, que não é mais chamado;
  `sub_82FD0C54`, uma instrução solta que ninguém chama; e duas funções de compensação de movimento do
  decodificador (`sub_83146E08`, `sub_831477A8`) em que v14-v24 são rascunho e as leituras ficam em caminhos que as
  condições de entrada excluem (o resultado só é usado quando `r9 != 0`, e o caminho só existe com `r9 == 0`).
- Quem lê `ctx.lr`: só o rastreador de Direct3D (desligado) e a atribuição de chamador do perfil do console. Nenhum
  hook do app lê cr, xer, ctr ou registrador não volátil.

**Conferido no PC** (`ds2_082`, `ds2_086`; a linha `[enh]` diz `optimised codegen (non-volatile locals, no lr
stores, modeless fp moves)`): título, avisos, lista de saves com os quatro retratos, Majula em 3D (andar, girar a
câmera, atacar, abrir o menu; `out/pc/captures/p3.png` a `p7.png`), "New Game" e o vídeo de abertura com imagem
limpa e legendas (`z1.png` a `z4.png`: é o teste das marcas do decodificador). Nenhum `FATAL`; a única diferença dos
autotestes é a de antes (`VS identity mismatch`, igual no clássico).

### Três tropeços das ferramentas do PC, achados no caminho

1. **65.535 arquivos `MEM*.tmp` na pasta temporária.** A primeira sessão com o código novo ficou 19 minutos sem
   apresentar um quadro de 3D, com três threads a 85 %. Não era o código: cada pipeline nova levava exatos 46 s
   (`PipelineFor 45898152 us`), e as do pré-aquecimento passaram de 0,35 s (às 19 h) para 1,6 s (20h50) e 48 s
   (23h20). `tools/win/sample_threads.py` (novo, abaixo) mostrou a thread do driver dentro de `NtCreateFile`, e a
   pilha dela apontava para `%TEMP%\MEMxxxx.tmp`: o Mesa no Windows cria um arquivo temporário para cada fluxo de
   memória (aqui com `spec[0] = 0x0000a320`), `GetTempFileName` só tem 65.535 nomes, e as sessões de teste, paradas
   por fora, tinham deixado todos lá em três dias. Apagados (eram 2 MB), e `tools/win/boot_test.ps1` agora dá a cada
   sessão a própria pasta temporária (`out\pc\tmp`), esvaziada no início: 456 pipelines pré-aquecidas em 30 s.
   Os tempos de PC medidos na tarde de 11/10 (os 37 s por retrato no lavapipe, por exemplo) estavam inflados por isso.
2. **`guided_session.ps1 -Pad` nunca funcionou.** A variável interna `$pad` é a mesma que o parâmetro `-Pad`
   (nomes do PowerShell não distinguem maiúsculas) e o sobrescrevia; o roteiro padrão (START três vezes, A a cada
   6 s) rodava sempre. Era ele que "carregava o save sozinho" quando eu tentava parar na lista. Corrigido.
3. **`tools/win/sample_threads.py`**: para uma sessão de PC que parou de andar, sem depurador instalado (o
   `lldb.exe` do LLVM daqui não abre: falta a DLL do Python 3.10). Suspende cada uma das threads mais ocupadas por um
   instante, lê o ponteiro de instrução e os endereços de retorno do topo da pilha e dá nome a eles com o
   `llvm-symbolizer` e o `ds2.pdb` (`--all-modules` mostra também as DLLs).

**Builds de Switch com o código otimizado** (2026-10-12, `out/build_switch_all.log`): `out\nx` em 58,1 min, NRO de
80,8 MB (o clássico tinha 105,6 MB); `out\nx_pgo` (instrumentado) em 83,8 min, 105,7 MB. `tools/check_switch_build.py`
passa nos dois: nenhum desvio para si mesmo no código recompilado (os cinco laços sem fim conhecidos do clássico
também sumiram: com as barreiras o GCC não os prova sem fim), nenhum multiplica-e-soma vetorial. ELFs em
`out/switch_elf/optimised_1/`. O perfil das coletas antigas (código clássico) foi para `out/pgo_classic`; o
`out\nx_rel` continua com o programa antigo (clássico, LTO + PGO), que vai no pacote como `ds2-opt.nro` só para
comparação até a coleta nova.

## Décimo oitavo build no console: o código novo funciona; a memória acabou aos 28 minutos (2026-10-12)

**Sessão normal** (`out/switch_logs/021/ds2_033.log`, `ds2.nro` com o código otimizado, sem LTO/PGO; 20 min, 11,8
milhões de desenhos, nenhum rejeitado, nenhum `FATAL`, retratos certos). Nos três pontos, contra o `ds2-opt.nro`
antigo (código clássico **com** LTO + PGO, sessão `ds2_032`):

| ponto | 540p antigo → novo | 720p antigo → novo | anel µs/desenho (540p) |
|---|---|---|---|
| 1 matagal | 16,6 → 19,5 | 15,4 → 15,3 | 36,6 → 51,1 |
| 2 casa | 17,0 → 17,6 | 15,4 → 15,9 | 42,3 → 50,9 |
| 3 vista de Majula | 18,1 → 19,4 | 17,2 → 17,8 | 38,9 → 50,5 |

O código do jogo otimizado, sozinho, já passa o clássico com LTO + PGO a 540p. A thread do anel ficou mais lenta
porque este programa não tem LTO/PGO (o renderizador é o mesmo código de antes); o próximo `ds2-opt.nro` junta as
duas coisas. A 720p o limite continua sendo a GPU.

**Sessão de coleta** (`ds2_034.log`, `ds2-pgo.nro`, 12:31 a 12:59 no relógio do PC): aos 28 minutos imagem
"fantasma", tela laranja ao trocar para 720p e o programa fechou (`rex_crash.log`: `abort()` vindo de
`Memory::AccessViolationCallback`, um acesso inválido do jogo). A ordem no log:

- 12:55:01, :15, :28 (a 540p, sem trocar nada): `nvkmd-switch: allocate memory failed: out of host memory`;
- 12:59:13, L3 + R3 para 720p: `targets: out of GPU memory for a 1040x1040 target ... dropping the cache was not
  enough` (e 1600x1600, 1280x720): alvos que não existem, tela laranja;
- 12:59:26, de volta a 540p: os alvos de 540p também não cabem, sobram alvos em escalas diferentes
  (`targets at different render scales (cause 41)`, 11.516 desenhos rejeitados): a imagem fantasma;
- 12:59:31: o jogo acessa memória inválida e o programa fecha.

Não é o código gerado: é **memória**. No Switch a memória de GPU do NVK sai do heap do processo, e a linha
`C3 GPU memory` dá o que o driver acha que ainda há: usado 664 → 759 → 852 MB, "orçamento" 1.092 → 1.052 → 1.133 MB,
ou seja, só ~280 MB livres no processo inteiro às 12:58 (na sessão normal, 299 MB aos 13 min; na coleta clássica de
27 min, 386 MB aos 15 min). O que cresce é o cache de texturas (151 → 252 → 330 MB, limite de 512), à medida que se
anda pelo jogo; trocar de resolução precisa de mais ~100 MB de uma vez (alvos de 172,6 MB a 540p, 273,4 MB a 720p).
Qualquer sessão longa chegaria lá, com qualquer um dos programas.

Onde estava a memória: no Horizon o heap inteiro é memória comprometida, e a pilha de host de cada thread do jogo é
um bloco do heap de **16 MB** (`params.stack_size = 16_MiB` em `sdk/src/system/xthread.cpp`). O DS2 mantém umas 50
threads vivas (`stacks: 50 from the heap, 12 reused`; 61 threads no relatório de perfil): ~800 MB dos ~3,2 GB do
processo em pilhas que usam algumas centenas de KB cada. No SA2, com poucas threads, isso não aparecia.

Correção (`switch_guest_host_stack_mb`, quarta exceção `ds2-nx` do SDK; padrão 0 = automático): a pilha de host
passa a ser 16 vezes a pilha de guest da própria thread, no mínimo 4 MB e no máximo 16 MB (a thread principal fica
com 16 MB). As primeiras 96 threads criadas escrevem no log a pilha de guest e a de host
(`[threads] guest thread N: guest stack X KB, host stack Y MB`), para ajustar depois com números. O log também ganha
a memória do processo (`process memory A of B MB; heap: ...`, de `svcGetInfo` e `mallinfo`) junto da linha
`C3 GPU memory` e na mensagem de falta de memória de GPU.

**O perfil foi aproveitado.** A pasta `pgo/` do cartão tinha três arquivos: `ds2_pgo_1` e `_2` são os da coleta
antiga (idênticos byte a byte aos de `out/switch_logs/017/pgo`, do código clássico: não servem) e `ds2_pgo_3.stream`
é o último despejo completo desta sessão, escrito dois minutos antes da queda (o `.tmp` de 5 MB é o despejo que
estava sendo escrito). Juntado sozinho em `pgo\`: 354 `.gcda`, maior contagem de arco 9.065.595.789.

**Tropeço do build:** mexer num arquivo do SDK faz o build de PC religar o gerador e rodar o codegen de novo, que
reescreve os 331 arquivos gerados (o conteúdo final é o mesmo depois dos passos pós-codegen, mas a data muda e todos
os builds recompilam tudo; e o próprio build de PC falha uma vez, porque o `ds2_pch.h` muda depois de o cabeçalho
pré-compilado ser feito). Conferi que dois dos arquivos compilam para objetos idênticos aos de `out\nx` e devolvi
as datas antigas aos 331, para o `out\nx` recompilar só o que mudou.

**Décimo nono build** (2026-10-12): `out\nx` com a correção de memória em 3,7 min (80,9 MB); `out\nx_rel` com o
código otimizado, LTO e o perfil novo em 68,7 min (90,5 MB). `tools/check_switch_build.py` passa nos dois. O perfil
casa com o código (um arquivo recompilado com `-fdump-ipa-profile-blocks`: contagens lidas, nenhum aviso de
divergência). ELFs em `out/switch_elf/optimised_2/`. Um dado do perfil para depois: o arco mais executado do programa
inteiro (9.065.595.789 vezes em 26 min) é o laço de espera de `sub_829B9C60`, a função do Direct3D que gira
esperando o anel da GPU andar: a thread principal queima CPU ali enquanto o renderizador está atrasado.

## Décimo nono build no console: memória resolvida; a notificação de conquista derruba o apresentador (2026-10-12)

Sessão de 46 minutos com o `ds2-opt.nro` novo (código otimizado + LTO + PGO; `out/switch_logs/022/ds2_035.1.log` e
`ds2_035.log`, 15:06 a 15:56 no relógio do PC). Nenhum `FATAL`, nenhuma falha de memória, 12 trocas de resolução.

**Os três pontos** (antes dos 21 minutos), contra o `ds2-opt.nro` antigo (clássico + LTO + PGO) e o `ds2.nro` do
décimo oitavo build (otimizado sem LTO/PGO):

| ponto | 540p: antigo → sem LTO/PGO → novo | 720p: antigo → novo | GPU ociosa a 540p |
|---|---|---|---|
| 1 matagal | 16,6 → 19,5 → **20,3** | 15,4 → 15,3 | 1,9 ms |
| 2 casa | 17,0 → 17,6 → **20,1** | 15,4 → 16,1 | 1,4 ms |
| 3 vista de Majula | 18,1 → 19,4 → **22,0** | 17,2 → 18,8 | 6,3 ms |

A 540p são +18 a +22 % sobre o programa antigo, e a GPU deixou de ficar parada: com 45 a 50 ms de GPU por quadro
(cena 25 a 32 ms), o limite a 540p voltou a ser a GPU. A etapa 3 (corte por alfa nos shaders) é a próxima.

**Memória.** As pilhas: 68 threads com pilha de guest de 16 KB, 8 de 128 KB, 7 de 64 KB, 6 de 256 KB, 6 menores, todas
com 4 MB de host; a principal (256 KB) com 16 MB. Nenhuma estourou em 46 minutos. O log novo:

| hora | heap pego do sistema | alocado | livre dentro | pedaços livres | GPU | cache de texturas |
|---|---|---|---|---|---|---|
| 15:14 | 2.154 MB | 1.948 | 205 | 43.656 | 664 MB | 149 MB |
| 15:24 | 2.518 | 2.257 | 260 | 94.148 | 931 | 382 |
| 15:44 | 2.858 | 2.345 | 512 | 169.932 | 927 | 382 |

O cache de texturas parou em 382-386 MB (o gotejamento das frias começa a 75 % do limite de 512) e a GPU em ~930 MB.
Fora a GPU, o processo usa 1,3 a 1,4 GB e cresce devagar (+134 MB em 30 min); o heap fragmenta (170 mil pedaços
livres) e o que ele pega do sistema continua subindo (2.858 de ~3.190 MB aos 38 min). Sobrou margem nesta sessão, mas
uma sessão de várias horas ainda pode chegar ao teto: fica na lista (o pool de texturas está em 16 blocos de 32 MB,
86 % fragmentado, e 1.273 das 2.891 texturas do cache têm o mesmo conteúdo de outra).

**A perda de desempenho "ao longo do tempo" começa num instante exato: 15:28:29.**

    15:28:29.497 Achievement unlocked: 00000026
    15:28:29.552 Presenter: paint mode -> on the UI thread, on request (implicit vsync no, UI drawers 1)

O SDK mostra a conquista num diálogo ImGui. Um diálogo registra um "UI drawer" enquanto existe, e com um drawer
registrado o apresentador deixa de apresentar pela thread do jogo: quem pinta é a thread de interface, a pedido. O
sa2-nx já tinha medido isso (de ~18 para ~9 imagens por segundo) e por isso cria o diálogo só na primeira conquista;
mas, criado, ele fica até o programa fechar. Daquele segundo em diante, nesta sessão:

| | antes | depois |
|---|---|---|
| envios à GPU por quadro (kickoffs) | 2 | 8 a 14 |
| consultas de fence por segundo | 25 a 40 | 450 a 790 |
| tempo esperando fences, por segundo | 2 a 70 ms | 550 a 980 ms |
| apresentador: espera pelo envio de 3 quadros atrás | 0,03 ms | 6 a 8 ms (pior: 40) |
| thread do anel, µs por desenho | 45 a 50 | 90 a 145 |
| quadros/s (média de 10 min) | 19,5 a 21,3 | 12,3 a 13,8 |

e as janelas de 1 a 4 quadros/s que o usuário viu (15:32:37, 15:34:34, 15:44:07: 190 a 210 ms de "GPU por quadro"
com só ~20 ms de cena) são do mesmo regime. A sessão clássica de 27 minutos (`ds2_027`) também teve uma conquista,
mas era o programa instrumentado e ninguém mediu.

Correção (`ds2_achievement_toast`, padrão `false`): o app sobrepõe `CreateAchievementNotificationDialog()`, o ponto
de extensão que o SDK documenta ("returning nullptr disables notifications"), e não cria o diálogo. A conquista é
gravada do mesmo jeito; só não aparece o aviso na tela.

Outro dado do perfil de threads no fim da sessão: thread do anel a 78-83 % de um núcleo e a principal do jogo a
62 %, com 266-280 % de CPU total nos três núcleos.

**Vigésimo build** (2026-10-12): `out\nx` em 0,8 min (80,9 MB) e `out\nx_rel` em 41,8 min (90,5 MB), os dois com
`ds2_achievement_toast`; `tools/check_switch_build.py` passa nos dois. ELFs em `out/switch_elf/optimised_3/`.

## Vigésimo build no console: desempenho estável; som "robótico" e a prioridade do mixer do FMOD (2026-10-13)

Sessão de 48 minutos com o `ds2-opt.nro` (`out/switch_logs/023/ds2_036.1.log` e `ds2_036.log`), sem conquista nova.
O apresentador ficou o tempo todo no modo da thread do jogo e o desempenho não caiu: 19,8 / 22,4 / 19,1 / 22,3 / 18,5
quadros/s por faixas de dez minutos (na sessão anterior, 12 a 14 depois da conquista). A thread do anel ainda encarece
com o cache de texturas perto do limite (42 → 54 → 75 µs por desenho). Nenhum `FATAL`.

O usuário relata duas coisas: **som um pouco robótico e arrastado**, e **travadas em pontos-chave** (transições de
área).

**Som.** O FMOD pede dados ao decodificador XMA do SDK a cada bloco. Por janela de 10 s:

| sessão | pedidos (kicks) | decodificação na thread do XMA | janelas com o dado pronto mais de 10,6 ms depois do pedido | pior |
|---|---|---|---|---|
| `ds2_032` (10 min, começo do jogo) | 9.391 | 0,58 s | 0 de 60 | 9,6 ms |
| `ds2_035.1` (46 min) | 19.208 | 1,78 s | 49 de 161 | 52 ms |
| `ds2_036.1` (primeira metade) | 19.641 | 1,55 s | 32 de 162 | 36 ms |
| `ds2_036` (segunda metade) | 25.625 | 2,48 s | 64 de 122 | 51 ms |

Mais adiante no jogo há o dobro ou mais de vozes, e o áudio passa a chegar atrasado em 20 a 50 % das janelas. O
nfsmw-nx documenta o mesmo sintoma (`docs/audio-and-video.md`, "Robotic audio"): no Switch todas as threads do jogo
rodam na prioridade 0x3B, a única que o Horizon reveza (fatias de 10 ms); a thread de áudio do jogo espera a vez
atrás da principal e das outras ocupadas, o bloco de 256 amostras (5,33 ms) sai atrasado e o XAudio2 toca um buraco.
A correção que mais ajudou lá foi subir a prioridade dessa thread para 0x2D.

No DS2 a thread é a "FMOD mixer thread" do FMOD Ex 4.40. Achada pela string na imagem (`0x82047D1C`):

- `sub_828478D0` chama `FMOD::Thread::initThread` (`sub_8282C1B0`) com esse nome; o `initThread` copia o nome para o
  deslocamento 5 do objeto `Thread` e chama `FMOD_OS_Thread_Create` (`sub_82822F58`) com a rotina
  `FMOD::Thread::callback` = `sub_8282BF30` (`lis r11,-32125; addi r4,r11,-16592`);
- `FMOD_OS_Thread_Create` cria a thread suspensa, compara o nome com os seis nomes de threads do FMOD para escolher o
  núcleo (`XSetThreadProcessor`) e chama `SetThreadPriority` com a prioridade que o FMOD pediu: o próprio jogo pede
  prioridade alta para o mixer, e o SDK ignora (todas as threads de guest a 0x3B).

`app/src/ds2_audio_threads.cpp`: um hook em `sub_8282BF30` lê o nome na própria thread nova e, se for o mixer, sobe a
prioridade dela (`ds2_audio_mixer_priority`, 0x2D no Switch: abaixo do anel, 0x2C, e das threads de áudio do SDK,
0x2B). O mixer bloqueia entre um bloco e outro, então não prende o núcleo. `ds2_audio_stream_priority` faz o mesmo
para a "FMOD stream thread" e fica em 0 (desligado) até haver medida. O log diz cada thread do FMOD que nasce.

**Travadas.** As janelas de 10 s do log não mostram travadas de poucos segundos, e o detalhe por quadro (`[hitch]`)
tinha um teto de 1.000 linhas com limiar de 60 ms: a 20 quadros/s quase todo quadro de área pesada passa de 60 ms e o
teto acabava nos primeiros minutos. Agora um quadro de 400 ms ou mais é sempre escrito (com a mesma decomposição:
GPU, esperas, texturas, envios). O que se sabe: nas transições o jogo lê 60 a 80 MB do cartão em ~3 s (1.000 a 2.000
leituras de 128 KB, ~21 MB/s) e cria 250 a 270 texturas em 10 s. O próximo log diz se o tempo vai no renderizador ou
se é o jogo que para de mandar quadros.

**Sessão com overclock** (`ds2_037`, 6 min, `ds2.nro`): o relatório de perfil leu CPU 1.020 MHz / GPU 460,8 MHz /
memória 1.331 MHz em todos os 35 relatórios, isto é, os clocks de fábrica: o perfil de overclock não chegou a valer
para este programa (ou não vale para o título pelo qual ele é aberto).

**`-fno-trapping-math` saiu** dos builds otimizados (fica `-fno-math-errno`). A auditoria do sa2-nx
(`docs/cpu-rounds.md`): a opção deixa o compilador adiantar uma operação de ponto flutuante para antes de uma troca do
modo de flush do host, que ele não sabe que importa; rendia no máximo 0,5 % de um arquivo. Aqui havia mais um motivo:
o build de coleta não tinha a opção e o de uso tinha, uma diferença antes da leitura do perfil.

## Os repositórios de referência em 2026-10-13: o que há de novo

`_ref/sa2-nx` foi de `fdd6f14` (2026-10-08) para `d2c6e15` (versão 1.0.2 de pré-lançamento): 11 commits, 279
arquivos, 14 mil linhas. `_ref/nfsmw-nx`: 4 commits, só ferramentas de dublagem. `_ref/torchlight-nx` (novo): outro
port ReXGlue (Torchlight da XBLA), com o renderizador nativo em Vulkan.

O que serve ao DS2, por ordem de valor:

1. **Sessões longas** (`sa2_native_resolved_retire_idle`, sa2-nx 1.0.2): as texturas copiadas (`resolved_`) nunca eram
   liberadas; cada cena deixa as suas e depois de uma a duas horas a memória de GPU acaba (sombras erradas, depois
   texturas pretas e geometria faltando; o nfsmw-nx tem o mesmo relato). Passam a ser liberadas depois de um minuto
   sem cópia nem leitura. O DS2 usa o mesmo renderizador e a memória aqui já é apertada: é a primeira coisa a portar.
2. **As três rodadas de CPU de 2026-10-12** (`docs/cpu-rounds.md`; o autor estima 15 % no console): menos trocas do
   modo de ponto flutuante do host (`fp_compares_modeless`, `fp_mode_lookahead`, provas pelo fluxo de controle),
   compare-and-swap do guest sem barreira, sem ponteiro de quadro, constantes da `.rdata` dobradas (16 mil cargas),
   chamadas indiretas com um alvo usual viradas em chamadas diretas guardadas (81 a 85 % das que rodam), registradores
   como locais em laços e em funções folha, formas NEON dos auxiliares de VMX, seções críticas sem a busca da thread,
   `fctid`/`fctiw` arredondando como o PowerPC (uma correção, não só velocidade), e o limite de embutimento do link
   de 30 para 64. Quase tudo depende do SDK e do gerador novos: é atualizar `sdk/` para o do sa2-nx (mantendo as
   exceções `ds2-nx`), portar as ferramentas, gerar de novo, auditar e coletar perfil outra vez.
3. **Áudio** (nfsmw-nx): além da prioridade, versões nativas das funções de áudio mais quentes (reamostradores,
   filtro, soma com ganho: -17,5 % de CPU na thread de áudio de lá). No DS2 a thread do mixer do FMOD gasta 23 a 48 %
   de um núcleo, então há o que ganhar; precisa do perfil com pilhas para achar as funções.
4. O que **não** se aplica ou foi descartado lá: `-fno-trapping-math` (acima), a remoção das gravações mortas de
   registradores não voláteis no prólogo (mudou a imagem no SA2: o jogo lê uma palavra de pilha nunca inicializada e
   depende dos zeros) e do registrador de ligação.

**Vigésimo primeiro build** (2026-10-13): `out\nx` em 3,3 min (80,9 MB) e `out\nx_rel` em 68,1 min (90,3 MB, todo o
código do jogo recompilado sem `-fno-trapping-math`); `tools/check_switch_build.py` passa nos dois. No PC o gancho
reconhece as quatro threads do FMOD (`FMOD mixer thread`, `FMOD stream thread`, `FMOD nonblocking thread (0)`,
`FMOD file thread`), e uma amostragem da thread que mistura confirmou a cadeia `FMOD::Thread::callback` →
`sub_82847B18` (a função de thread do mixer, vizinha de quem o cria) → ... → `sub_8283AD00` (a rede de DSP). ELFs em
`out/switch_elf/optimised_4/`.

## Vigésimo primeiro build no console; sessões longas e a criação de threads (2026-10-15)

Sessão de 9 minutos com o `ds2-opt.nro` (`out/switch_logs/024/ds2_040.log`). O usuário: menos som robótico e menos
travadas repentinas; as travadas de transição de área continuam; na área da água com os cavaleiros, 13-15 quadros/s.

- **Mixer:** `[audio] FMOD mixer thread: Horizon priority 0x2d set`.
- **Quadros longos** (o registro novo, de 400 ms ou mais sempre, e o de 60 ms com teto que agora não se esgotou):
  só 7 quadros acima de 250 ms em 9 minutos, os maiores de 652 e 446 ms na entrada no jogo. Os das transições
  (15:26:15, 278 ms; 15:27:14, 268 ms; também 120 e 203 ms logo antes) têm **GPU 1,4 ms, uma cópia, nenhum Swap**:
  o renderizador esperava o jogo, que parou de mandar quadros. E dentro de cada um o jogo cria **cinco threads
  seguidas** (`[threads] guest thread 49` a `53` às 15:26:15,33-15,57; `54` a `58` às 15:27:14,39-14,61).
- **Área dos cavaleiros:** GPU 58-62 ms por quadro, dos quais 26 ms parada (`gap`), cena 20 ms; anel 35-37 ms. O
  limite ali é a CPU (o jogo), não a GPU.

**O que muda neste build:**

- **Sessões longas** (portado do sa2-nx 1.0.2, `3aff36c`, `ds2_native_resolved_retire_idle`): as texturas copiadas
  (`resolved_`) eram guardadas a sessão inteira, uma por endereço que já recebeu uma cópia; o jogo libera a textura
  de um alvo quando a cena acaba e a próxima nasce noutro endereço, e elas se acumulam até a memória de GPU acabar
  (no SA2 e no NFSMW: sombras erradas, depois texturas pretas e geometria faltando depois de uma a duas horas). Agora
  uma que não recebe cópia nem é lida por `ds2_native_resolved_idle_frames` quadros (3.600) é liberada, e se a GPU
  ficar sem memória as paradas há 120 quadros saem no Swap seguinte. Relatório a cada 600 quadros
  (`targets: N resolved textures and P parked hold X MB; R released ...`). No PC, com 120 quadros para forçar:
  24 liberadas (43 MB) em 5 minutos, nenhuma busca num endereço liberado, nenhum `FATAL`.
- **Criação de threads** (SDK, `threading_posix.cpp`, exceção `ds2-nx`): as pilhas de 4 MB das threads do jogo
  passam a usar o pool de pilhas do sa2-nx (era só para as de 16 MB: mínimo 8 MB, quatro blocos), com até dezesseis
  blocos; antes cada thread nova pedia 4 MB a um heap com 170 mil pedaços livres e os devolvia um segundo depois de
  acabar. E cada criação de 4 ms ou mais vai para o log (`[threads] creating a thread ... took X ms`): o próximo log
  diz se a travada é o custo das cinco criações ou outra coisa que o jogo faz no mesmo momento.

**As referências, 2026-10-15.** O `torchlight-nx` saiu em 1.0.1 e 1.0.2:

- 1.0.1: um travamento "device lost" em cenas cheias (a fila de comandos do NVK cheia na submissão), corrigido com um
  patch do Mesa (`switch/patches/mesa-switch-gpfifo-reserve.patch`). O DS2 usa o mesmo driver: vale portar se um dia
  aparecer um `device lost`.
- 1.0.2: portou as ferramentas da segunda e terceira rodadas de CPU do sa2-nx como passos sobre o código gerado,
  sem trocar o SDK inteiro: chamadas indiretas promovidas a diretas com guarda (a partir de um censo feito no PC),
  constantes de leitura dobradas, cargas de ponto flutuante mais rápidas, registradores como locais em funções folha
  e em laços sem chamada. É o caminho para o DS2 também: as ferramentas do sa2-nx (`tools/ro_constants.py`,
  `float_loads.py`, `leaf_locals.py`, `loop_locals.py`, `indirect_calls.py`, `indirect_promotions.py`) precisam das
  macros novas no `ds2_pch.h` (`REX_RO_U32`, `REX_LOAD_D_F32`, `REX_LEAF_LOCAL`, `REX_LOOP_IN`,
  `REX_CALL_INDIRECT_FUNC_AT`, `REX_PROMOTED_IS`...) e da conferência do lado do PC (`RexRoMismatch`, o censo de
  chamadas indiretas). Como a área dos cavaleiros mostra, é a CPU que falta.
