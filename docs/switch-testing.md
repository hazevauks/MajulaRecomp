# Testando o Dark Souls II (MajulaRecomp) no Switch

Este é o **primeiro build para o console**. No PC o jogo chegou ao primeiro mapa com a imagem certa, mas só pelo
Vulkan por software; no Switch nada foi testado ainda. O objetivo deste teste é saber até onde ele vai e trazer os
logs de volta.

## Vigésimo segundo build (2026-10-15): sessões longas e as travadas de área

Copie por cima: `ds2-opt.nro` e `ds2.nro`.

O que muda:

- **Sessões longas.** O renderizador guardava para sempre as imagens que o jogo copia dos alvos de renderização
  (sombras, efeitos), uma por endereço; em jogos com o mesmo renderizador isso acaba com a memória depois de uma a
  duas horas (sombras erradas, texturas pretas). Agora as que ficam um minuto sem uso são liberadas.
- **Travadas de área.** Em cada uma o jogo para de mandar quadros enquanto cria cinco threads seguidas. As pilhas
  dessas threads passam a ser reaproveitadas em vez de pedidas de novo à memória, e o log passa a dizer quanto tempo
  cada criação leva.

**Teste, com o `ds2-opt.nro`:** uma sessão longa (uma hora ou mais, se der), passando por várias transições de área.
Repare se as travadas de área mudaram e se algo estranho aparece com o tempo (sombras, texturas pretas, coisas
faltando).

Depois a pasta `logs/` (com `rex` dentro).

## Vigésimo primeiro build (2026-10-13): o som robótico e o registro das travadas

Copie por cima: `ds2-opt.nro` e `ds2.nro`.

O que muda:

- **Som.** A thread que mistura o som do jogo (o mixer do FMOD) rodava na mesma prioridade das threads ocupadas do
  jogo e esperava a vez delas; os blocos de som saíam atrasados (som robótico, arrastado), e piora mais adiante no
  jogo, onde há mais vozes. Ela agora tem prioridade acima delas.
- **Travadas.** Ainda sem correção: o log não registrava os quadros longos depois dos primeiros minutos. Agora
  registra todo quadro de 0,4 s ou mais, com o que o renderizador fazia nele.

**Teste, com o `ds2-opt.nro`:** jogue pelas áreas onde o som estava pior e passe por algumas transições de área onde
travava. Repare (1) se o som melhorou e (2) mais ou menos em que momentos trava (não precisa anotar a hora: o log
marca).

Se o som piorar ou algo novo aparecer, a prioridade volta ao que era com esta linha no fim do `ds2.toml`:
`ds2_audio_mixer_priority = 0`. Se a música ou as vozes ainda quebrarem, vale testar também
`ds2_audio_stream_priority = 45`.

Depois a pasta `logs/` (com a pasta `rex` dentro: o `rex_profile.log` é o da última sessão, então a sessão principal
deve ser a última a rodar).

## Vigésimo build (2026-10-12): a queda de desempenho depois de uma conquista

Copie por cima: `ds2-opt.nro` e `ds2.nro`.

O que muda: na sessão de 46 minutos o desempenho caiu de ~20 para ~12 quadros/s num instante exato, o da primeira
conquista desbloqueada. O aviso de conquista na tela deixava o programa num modo de apresentação mais lento até ser
fechado (e era dele que vinham os trechos a 1-3 quadros/s). O aviso foi desligado; a conquista é gravada igual.

**Teste, com o `ds2-opt.nro`:** jogue normalmente por 40 minutos ou mais e repare se o desempenho se mantém do
começo ao fim. Trocar de resolução continua valendo (L3 + R3).

As conquistas são do perfil, não do save: as que já saíram não saem de novo num save novo. Elas ficam no cartão em
`switch/ds2-nx/ds2/achievements/465307E4.toml`. Para o teste passar por uma conquista de propósito, há dois jeitos:

- jogar até uma que ainda não saiu (o próximo chefe, por exemplo); ou
- antes de abrir o jogo, renomear esse arquivo para `465307E4.toml.bak`: o jogo passa a achar que nenhuma saiu, e as
  do começo (num save novo) saem outra vez. Depois do teste, apague o `465307E4.toml` novo e devolva o nome ao
  `.bak`, para ficar com as datas originais. Os saves não são tocados.

Com a correção o aviso nem chega a ser criado, então mesmo uma sessão longa sem conquista já serve; passar por uma
só confirma que o problema era esse.

Depois a pasta `logs/`.

## Décimo nono build (2026-10-12): memória para sessões longas e o novo programa otimizado

Copie por cima: `ds2.nro` e `ds2-opt.nro`. Pode apagar do cartão o `ds2-pgo.nro` (não é preciso coletar de novo
agora) e a pasta `pgo/` (os arquivos dela já foram usados).

O que muda:

- **A queda aos 28 minutos era falta de memória**, não o código novo: cada thread do jogo reservava 16 MB de pilha
  (umas 50 threads, 800 MB), e com o cache de texturas crescendo ao longo da sessão a memória do processo acabava.
  As pilhas agora têm o tamanho que cada thread precisa. Vale para os dois programas.
- `ds2-opt.nro` agora é o **código novo com LTO + PGO**, feito com o perfil da sua coleta (os 26 minutos antes da
  queda foram aproveitados).

| arquivo | o que é |
|---|---|
| `ds2-opt.nro` | o programa a usar: código otimizado + LTO + PGO |
| `ds2.nro` | o mesmo código sem LTO/PGO (reserva, e para comparar) |

**Teste, com o `ds2-opt.nro`:**

1. Os três pontos parados de sempre (matagal, casa das firekeepers, vista de Majula), ~30 s a 540p, L3 + R3,
   ~30 s a 720p.
2. Uma **sessão longa, de 40 minutos ou mais**, passando por várias áreas, e trocando de resolução algumas vezes
   pelo caminho (foi o que derrubou da outra vez). É o teste da correção de memória.

Se alguma thread do jogo precisar de mais pilha do que recebeu, o programa fecha de repente (sem imagem estranha
antes). Se isso acontecer, mande os logs e, para continuar jogando, crie na pasta do jogo um `ds2.toml` com a linha
`switch_guest_host_stack_mb = 16` no fim (volta ao comportamento antigo).

Depois a pasta `logs/`.

## Décimo oitavo build (2026-10-12): código do jogo recompilado de outro jeito

Copie por cima: `ds2.nro`, `ds2-pgo.nro` e `ds2-opt.nro` (o `ds2.toml` e a pasta `configs/` não mudaram).

O que é cada programa agora:

| arquivo | o que é |
|---|---|
| `ds2.nro` | **o novo**: o código do jogo traduzido com as opções de velocidade (registradores do PowerPC em variáveis locais, chamadas diretas entre funções). Ainda sem LTO/PGO |
| `ds2-pgo.nro` | o mesmo código novo, instrumentado, para a coleta de perfil. É lento de propósito |
| `ds2-opt.nro` | o **antigo** (décimo sétimo build: código clássico com LTO + PGO), só para comparar |

Esta é a mudança mais funda desde o primeiro build: todas as 83 mil funções do jogo foram geradas de novo. No PC o
jogo passou pelos menus, lista de saves, Majula e pelo vídeo de abertura de um jogo novo, mas o compilador do Switch
é outro, então o primeiro teste é de estabilidade.

**Teste 1, `ds2.nro` (estabilidade e velocidade):**

1. Abra, entre na lista de saves (os quatro retratos têm de aparecer) e carregue um save.
2. Os três pontos parados de sempre (matagal, casa das firekeepers, vista de Majula), ~30 s a 540p, L3 + R3,
   ~30 s a 720p. É a comparação com o `ds2-opt.nro` antigo, que você já mediu; não precisa repetir com ele.
3. Jogue uns 10 minutos normalmente: combate, uma fogueira, um carregamento de área.
4. Se puder, comece um **jogo novo** e veja o vídeo de abertura por alguns segundos (o decodificador de vídeo foi
   uma das partes que precisou de cuidado). Pode pular depois.

Se o `ds2.nro` travar ou fechar em qualquer ponto, pare aí e mande a pasta `logs/` (e as pastas de relatório do
Atmosphère, se houver): não adianta coletar perfil de um programa que não está certo.

**Teste 2, `ds2-pgo.nro` (só se o teste 1 passou):** a coleta de perfil, como da outra vez: jogue 20 a 30 minutos
de jogo variado (do início até onde der, com combate e carregamentos) e saia pelo menu HOME fechando o programa.
O perfil antigo não serve para o código novo; é esta coleta que vai alimentar o próximo `ds2-opt.nro`.

Depois: a pasta `logs/` e a pasta `pgo/` que aparece ao lado do programa.

## Décimo sétimo build (2026-10-11): retratos dos saves e mais um corte na GPU

Copie por cima: `ds2-opt.nro` e `ds2.nro` (o `ds2.toml` e a pasta `configs/` são os do décimo sexto build; se o
`ds2.toml` do cartão ainda for o `ds2_melhorias_desligadas` do último teste, volte o do pacote).

O que muda:

- **Retratos na lista de saves:** o jogo desenha cada retrato numa textura pequena e lê os pixels pela CPU; o
  renderizador não devolvia essas texturas para a memória do jogo. Agora devolve.
- **GPU:** a conversão de formato da imagem HDR que ainda sobrava (cerca de 5 ms por quadro a 540p) ganhou um atalho.

O teste, com o `ds2-opt.nro`:

1. Abra a lista de saves e veja os quatro retratos (uma foto ajuda). Entre num save, salve/saia para o título e
   abra a lista de novo.
2. Os três pontos parados de sempre (matagal, casa das firekeepers no mesmo lugar da primeira vez, vista de Majula),
   ~30 s a 540p, L3 + R3, ~30 s a 720p.

Depois a pasta `logs/`.

## Décimo sexto build (2026-10-11): perfil completo e conversões mais baratas

Copie por cima: `ds2-opt.nro`, `ds2.nro`, `ds2.toml` e a pasta `configs/`.

O que muda:

- `ds2-opt.nro`: PGO com as duas coletas (6,5 + 27 minutos) e LTO;
- `ds2.toml` (vale para os dois programas): as conversões de formato da imagem HDR, que custavam 14-17 ms de GPU por
  quadro a 540p, passam por um caminho mais barato. No PC a imagem saiu igual.

**Teste 1 (principal), `ds2-opt.nro`:** os mesmos três pontos parados da última vez (matagal do início, dentro da
casa das firekeepers, vista de Majula), ~30 s a 540p, L3 + R3, ~30 s a 720p, L3 + R3. Não precisa repetir com o
`ds2.nro`. Repare se a imagem tem algo estranho em cores, brilho ou neblina (é o que essas conversões tocam).

**Teste 2 (as miniaturas dos saves):** apague o `ds2.toml`, copie `configs/ds2_melhorias_desligadas.toml` para a
pasta do jogo, renomeie para `ds2.toml`, abra o `ds2.nro` e vá até a lista de saves. Os retratos saem certos ou
continuam com ruído? Uma foto ajuda. Depois volte o `ds2.toml` do pacote.

Mande a pasta `logs/` das duas sessões.

## Décimo quinto build (2026-10-11): PGO + LTO e a troca de resolução ao vivo

Copie por cima `ds2.nro` e `ds2-opt.nro` (o `ds2-pgo.nro` do build anterior pode ficar: serve para novas coletas).

| arquivo | o que é |
|---|---|
| `ds2.nro` | o build normal (sem LTO, sem PGO), a referência |
| `ds2-opt.nro` | o build otimizado: LTO + PGO com o perfil da sua sessão de 6,5 minutos |

**Trocar entre 540p e 720p sem fechar o jogo:** segure os dois analógicos pressionados (**L3 + R3**) por um
segundo. A resolução de render alterna entre 960x540 e 1280x720 na hora (a imagem pisca um quadro) e o log anota o
momento. Vale nos dois programas e dura até fechar o jogo; ao abrir de novo vale o que está no `ds2.toml` (540).
Dentro do jogo o R3 também trava a mira e o L3 também faz a ação dele, então use num lugar tranquilo.

O teste, sem overclock, de preferência nos mesmos lugares com os dois programas:

1. `ds2-opt.nro`, sessão longa (20-30 minutos), jogando normalmente. No meio dela, num ponto parado, deixe ~30 s a
   540p, aperte L3 + R3, deixe ~30 s a 720p e volte com L3 + R3.
2. `ds2.nro`, uns minutos no mesmo ponto parado (30 s a 540p, L3 + R3, 30 s a 720p), para a comparação.

O que observar no `ds2-opt.nro`: qualquer coisa diferente do `ds2.nro` além da velocidade (comportamento de
inimigos, física, colisão, som, travamentos). Este build reorganiza o código do jogo inteiro; se algo sair errado,
o `ds2.nro` continua sendo o de confiança.

Depois a pasta `logs/` (diga a ordem das sessões).

## Décimo quarto build (2026-10-11): funções preventivas e o início do PGO

Na pasta do pacote há agora até três programas. Todos usam o mesmo `ds2.toml`, os mesmos arquivos do jogo e os
mesmos saves; copie os três por cima (os que existirem):

| arquivo | o que é | para quê |
|---|---|---|
| `ds2.nro` | o build normal, com 368 funções pequenas do jogo declaradas de forma preventiva | jogar no dia a dia |
| `ds2-pgo.nro` | o build **instrumentado**: conta quais trechos do código rodam | uma sessão de coleta (abaixo) |
| `ds2-opt.nro` | o build com LTO (otimização em tempo de link), quando estiver na pasta | comparar com o `ds2.nro` |

No menu do homebrew, entre na pasta `ds2-nx` para escolher qual abrir.

### 1. `ds2.nro`

Jogue normalmente. As 368 funções novas são do mesmo tipo da que derrubou o jogo na floresta (funções vazias,
getters e atalhos de vtable que nada chama diretamente); foram declaradas antes de alguma fazer falta. Se o jogo
fechar e voltar ao menu do console, a pasta `logs/` continua dizendo qual endereço faltou.

### 2. A sessão de coleta com o `ds2-pgo.nro`

Este build é **mais lento** (cada desvio do código é contado) e a cada 2 minutos dá uma engasgada de alguns segundos
enquanto grava as contagens no cartão. É só para coletar; não serve para medir desempenho.

- Abra o `ds2-pgo.nro` e jogue de 15 a 20 minutos do jeito mais variado que der: menus, uma fogueira, andar, lutar,
  morrer, uma área aberta (Majula) e uma fechada (a floresta). O que não for exercitado não é otimizado.
- Saia pelo menu do jogo ou pelo HOME (ele grava ao perder o foco e ao sair).
- Mande a pasta `sdmc:/switch/ds2-nx/pgo/` (arquivos `ds2_pgo_<N>.stream`, um por sessão) junto com a `logs/`.

Com esses arquivos eu gero o build otimizado pelo perfil.

### 3. `ds2-opt.nro` (se estiver na pasta)

Mesmo roteiro de medição de sempre, sem overclock: ~30 s parado num ponto e um trecho andando, uma vez com o
`ds2.nro` e uma com o `ds2-opt.nro`, no mesmo lugar. Diga qual sessão foi qual.

## Décimo terceiro build (2026-10-10): a queda na Forest of Fallen Giants

Copie por cima: `ds2.nro`.

O jogo fechou porque chamou uma função dele mesmo que o recompilador não tinha encontrado (ela só é chamada por um
ponteiro montado em código). Essa e a irmã dela foram adicionadas. Pode haver outras do mesmo tipo em áreas ainda não
visitadas: se o jogo fechar de novo e voltar ao menu do console, mande a pasta `logs/` (o `logs/rex/rex_crash.log` e
o fim do `ds2_NNN.log` dizem qual foi).

Continue de onde parou (o save fica antes da primeira fogueira da floresta) e siga jogando normalmente.

## Décimo segundo build (2026-10-10): rodada 5, CPU do renderizador (índices)

Copie por cima: `ds2.nro`, `ds2.toml` e a pasta `configs/` (o `ds2.toml` volta a não amostrar pilhas).

O perfil mostrou que a thread do renderizador gastava 28 % do tempo convertendo índices de geometria um por um,
porque o jogo usa um formato (tiras com reinício) que o caminho rápido herdado não aceitava. Agora aceita.

Jogue normalmente, sem overclock; uns 30 s parado em algum ponto e o resto andando. O que observar: geometria
faltando, triângulos esticados pela tela ou objetos deformados (seria sinal de erro nessa conversão).

Depois a pasta `logs/`.

## Rodada de perfil de CPU (2026-10-10): só o `ds2.toml` muda

Copie por cima **só o `ds2.toml`** (o `ds2.nro` é o mesmo do décimo primeiro build; não troque).

Depois das últimas rodadas o limite passou a ser o processador: o jogo usa quase os 3 núcleos disponíveis. Este
`ds2.toml` liga a amostragem das pilhas de CPU, que diz em quais funções o tempo vai. O jogo fica um pouco mais
lento enquanto ela está ligada.

Uma sessão de ~3 minutos, sem overclock:

1. ~1 minuto parado na fogueira;
2. ~2 minutos na área do tutorial em que você esteve (andando e lutando normalmente).

Depois a pasta `logs/` (o arquivo que importa é `logs/rex/rex_profile.log`).

Desde o décimo segundo build o `ds2.toml` normal já vem sem a amostragem; ela fica em `configs/ds2_perfil.toml`.

## Décimo primeiro build (2026-10-10): depois da queda do sistema

Copie por cima: `ds2.nro`.

A tela de erro do Atmosphère do último teste veio de um módulo do próprio sistema (`pcv`, que cuida de clocks e
tensões): ele ficou sem memória para sessões e derrubou outros três módulos junto, entre eles o seu sysmodule de
clocks. Não dá para afirmar quem causou. O jogo era um dos programas com sessões abertas nesse módulo (só para
anotar os clocks no log), então este build deixou de mantê-las abertas.

- Jogue normalmente; se a tela de erro voltar, mande de novo as pastas `crash_reports` e `fatal_reports` e me diga
  o que estava aberto (barra de status, menu do sysmodule de clocks).
- Se voltar, vale um teste com a barra de status desligada para isolar.
- As sombras perto da casa das firekeepers e da fogueira continuam valendo a conferida.

## Décimo build (2026-10-10): rodada 4 de desempenho (sombras das luzes)

Copie por cima: `ds2.nro` (o `ds2.toml` e a pasta `configs/` continuam os mesmos).

O que muda: olhando para a casa das firekeepers o jogo caía para ~8 quadros/s porque, para cada luz com sombra, o
renderizador convertia o buffer de sombras inteiro de ida e volta. Essas limpezas agora são reconhecidas e feitas
direto. No PC as conversões nessa vista caíram de ~170 para ~25 pequenas por quadro.

Mesmo roteiro do último teste, a 540p e sem overclock:

- ~30 s parado olhando para a casa das firekeepers e ~30 s olhando para a saída (as fogwalls);
- repare nas **sombras** perto da casa e das tochas/fogueira: alguma sombra faltando, piscando ou no lugar errado?

Depois a pasta `logs/`.

## Nono build (2026-10-10): correção dos "fantasmas" a 540p

Copie por cima: `ds2.nro` (o `ds2.toml` e a pasta `configs/` continuam os mesmos).

No oitavo build as cores voltaram, mas apareceu uma segunda silhueta do personagem deslocada e a cena com aspecto
duplicado. Causa: a 540p os efeitos de tela cheia do jogo (sombras e luz) liam a posição do pixel na escala errada e
saíam ampliados em 4/3. Corrigido; no PC a imagem a 540p agora bate com a de 720p.

Na primeira vez o jogo pode engasgar um pouco mais nos primeiros minutos (os shaders são preparados de novo para
540p; depois ficam guardados).

O que observar, a 540p e sem overclock:

- o fantasma sumiu? As sombras do personagem e do cenário estão no lugar?
- gire a câmera para os dois lados (casa das firekeepers e direção oposta) e fique uns 30 s parado em cada um.

Depois a pasta `logs/`.

## Oitavo build (2026-10-10): correção da tela amarela a 540p

Copie por cima: `ds2.nro` (o `ds2.toml` e a pasta `configs/` são os mesmos do sétimo build).

No sétimo build a cena saiu amarela/verde no console, com o HUD certo: o driver de vídeo do Switch arredonda o
tamanho do viewport para pixels inteiros, e o desenho de 1 linha da tabela de tone mapping (0,75 pixel de altura a
540p) sumia. No PC isso não acontece, por isso passou no teste daqui. Agora o renderizador só envia viewports de
pixels inteiros.

Uma sessão: até a fogueira, ~1 minuto parado, sem overclock. As cores voltaram ao normal? O texto e o HUD a 540p
incomodam? Depois a pasta `logs/`.

Se ainda sair amarelo, volte a 720p (abaixo) para continuar jogando e me mande o log mesmo assim.

## Sétimo build (2026-10-10): rodada 3, 540p no modo portátil

Copie por cima: `ds2.nro`, `ds2.toml` e a pasta `configs/`.

O que muda: o `ds2.toml` deste pacote renderiza a **960x540** no modo portátil (56 % dos pixels de 720p) e amplia para
a tela. No PC a imagem ficou certa depois de uma correção (a tabela de tone mapping não era desenhada a 540p); o
ganho em quadros por segundo só o console diz. A interface também sai a 540p, então o texto fica um pouco menos
nítido.

Uma sessão basta: com o `ds2.toml` do pacote, vá até a fogueira e fique ~1 minuto parado, sem overclock. Repare:

- as cores da cena estão normais (nada de tela amarela/verde ou escura demais)?
- o texto e o HUD continuam legíveis? A perda de nitidez incomoda?

Para voltar a 720p: apague o `ds2.toml`, copie `configs/ds2_teste_720p.toml` para a pasta do jogo e renomeie para
`ds2.toml`.

Depois a pasta `logs/`, como sempre.

## Sexto build (2026-10-10): rodada 2 de desempenho

Copie por cima: `ds2.nro`, `ds2.toml` e a pasta `configs/`.

O que muda: as limpezas de tela que o jogo faz por superfícies 4x "apelidadas" passam a ser feitas direto nos alvos
normais, sem converter o alvo inteiro depois. No PC as conversões de profundidade do alvo inteiro caíram de ~6 para 2
por quadro e a imagem ficou igual; o ganho em milissegundos só o console diz.

Duas sessões na fogueira, ~1 minuto parado cada, sem overclock:

1. **Sessão A:** com o `ds2.toml` do pacote. Repare na imagem: céu/fundo, sombras e neblina iguais a antes?
2. **Sessão B:** apague o `ds2.toml`, copie `configs/ds2_teste_sem_atalho_clear.toml` para a pasta do jogo e renomeie
   para `ds2.toml` (é o comportamento do build anterior, para comparar).

Depois a pasta `logs/`, como sempre.

## Quinto build (2026-10-10)

Copie por cima: `ds2.nro`, `ds2.toml` e a pasta `configs/`.

- **ZCULL ligado por padrão** no alvo de profundidade da cena (o teste B da rodada anterior: 137,9 → 127,2 ms de GPU
  por quadro, 7,3 → 7,9 quadros/s). Se na sessão B você viu algo errado na imagem, me diga.
- **Corrigida a cópia rejeitada da área da fogueira:** a cópia da cena para uma textura de 8 bits (usada pelo efeito
  de calor do fogo) não era feita. Repare se o ar em volta da fogueira agora ondula.
- Teste: uma sessão na fogueira, ~1 minuto parado, sem overclock, e a pasta `logs/`.

## Quarto build (2026-10-10): primeira rodada de desempenho

Copie por cima: `ds2.nro`, `ds2.toml` e a pasta `configs/`.

Este build não fica mais rápido por si só: ele mede melhor e traz um teste. São **duas sessões no mesmo lugar** (a
primeira fogueira; com o save, "Continue" já abre lá), sem overclock, cada uma com ~1 minuto parado olhando a cena:

1. **Sessão A:** com o `ds2.toml` deste pacote, como está.
2. **Sessão B:** apague o `ds2.toml` da pasta do jogo, copie `configs/ds2_teste_zcull.toml` para lá e **renomeie a
   cópia para `ds2.toml`**. (O jogo só lê o arquivo com esse nome.) Rode de novo no mesmo lugar. Repare também se a
   imagem continua certa, principalmente sombras e bordas dos objetos.

Depois mande a pasta `logs/` e, se puder, a pasta de saves `switch/ds2-nx/ds2/` do cartão: com o seu save eu consigo
abrir essa mesma área no PC para investigar.

## Terceiro build (2026-10-10)

Copie por cima dos antigos: `ds2.nro`, **`ds2.toml`** e a pasta `configs/`. O `game_root/` não muda.

- **Teclado para o nome do personagem:** quando o jogo pedir um texto, abre o teclado do próprio Switch. Ainda não foi
  testado em console: se o teclado não abrir, travar ou o nome não for aceito, me diga exatamente o que aconteceu.
- **Ícone** novo no menu de homebrew.
- **Medição ligada no `ds2.toml` normal:** o jogo só lê o arquivo chamado `ds2.toml`; um `ds2_perfil.toml` ao lado do
  `.nro` é ignorado (pode apagar). Por isso o `ds2.toml` deste build já traz o tempo de GPU por categoria ligado. Basta
  jogar: entre no mapa, ande um pouco, fique uns 30 s parado olhando a cena, e me mande a pasta `logs/`.
- Sem overclock, por favor, para os números serem comparáveis (o primeiro log, com overclock leve, eu desconsiderei).

## Segundo build (2026-10-10)

Só o `ds2.nro` e a pasta `configs/` mudaram: copie esses dois por cima (o `game_root/` não precisa ser copiado de novo).

- **Corrigido:** a caixa "Save and quit game" (e o HUD desenhado depois dela) saía roxa/rosa. Agora é cinza-escura, com
  o botão YES azul e o NO laranja.
- **Pedido de medição:** para eu saber onde o tempo da GPU vai no mapa (o primeiro teste ficou em 5–7 quadros/s, com a
  GPU no limite), rode uma sessão com `configs/ds2_perfil.toml` copiado por cima do `ds2.toml`: entre no mapa, ande um
  pouco e fique uns 30 s parado olhando para a cena. Depois me mande a pasta `logs/` de novo. Essa configuração roda um
  pouco mais devagar que a normal.

## O que vai no cartão SD

Copie a pasta `ds2-nx` inteira para `sdmc:/switch/` (fica `sdmc:/switch/ds2-nx/`):

```
switch/ds2-nx/ds2.nro                  o jogo
switch/ds2-nx/ds2.toml                 configuração (log ligado nestes testes)
switch/ds2-nx/ds2_shaders.mesp         biblioteca de shaders do renderizador nativo
switch/ds2-nx/ds2_shaders.mesp.idx
switch/ds2-nx/game_root/               os arquivos do jogo, soltos (121 arquivos, ≈ 5,8 GB)
switch/ds2-nx/configs/                 configurações alternativas (ver abaixo)
```

O cartão é **FAT32**, que não aceita arquivo de 4 GB ou mais: por isso o jogo vai como pasta `game_root/` e não como
pacote único. O maior arquivo (`GameData1.bdt`) tem 2,8 GB e cabe. A cópia leva um tempo; confira no fim se os 121
arquivos chegaram (o Windows às vezes interrompe cópias grandes para cartões).

## Como abrir

O jogo precisa de mais memória do que um homebrew recebe aberto pelo Álbum. Abra o menu de homebrew **segurando R
enquanto inicia um jogo qualquer** (title override), e então escolha "Dark Souls II". Aberto pelo Álbum ele não deve
passar do início.

## O que observar

1. Abre? Aparece alguma imagem (avisos iniciais, logo da Havok, tela de título)?
2. Na tela de título, START (botão +) e depois A avançam pelos menus. "Continue" não existe no console (o save do PC
   não vai junto): escolha New Game.
3. Tem som?
4. Até onde chega: vídeo de abertura, primeiro mapa (um campo de capim escuro com pilares de pedra)?
5. Se travar ou fechar: em que tela, e depois de quanto tempo.

Uma foto ou vídeo curto da tela ajuda muito, principalmente se a imagem sair errada.

## O que me devolver

A pasta `sdmc:/switch/ds2-nx/logs/` inteira (ou só `logs/rex/`), depois de cada tentativa. Se o jogo fechar com erro,
o arquivo `logs/rex/rex_crash.log` diz onde. Se ele gerar relatório de erro do Atmosphère
(`sdmc:/atmosphere/crash_reports/`), o mais recente também serve.

## Configurações alternativas

Na pasta `configs/`; para usar uma, copie por cima do `ds2.toml`:

- `ds2_padrao.toml`: a configuração normal (igual ao `ds2.toml` enviado).
- `ds2_teste_720p.toml`: modo portátil a 1280x720, a resolução do próprio jogo (como nos builds até o sexto).
- `ds2_melhorias_desligadas.toml`: desliga todos os atalhos de CPU e GPU herdados dos outros portes. Se o jogo travar
  ou a imagem sair errada com a normal, tente esta e me diga se mudou.
- `ds2_gpu_emulada.toml`: usa a GPU emulada no lugar do renderizador nativo. É lentíssima no console; só serve para
  comparar a imagem numa tela parada.
