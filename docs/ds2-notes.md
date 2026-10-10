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
