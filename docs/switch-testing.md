# Testando o Dark Souls II (MajulaRecomp) no Switch

Este é o **primeiro build para o console**. No PC o jogo chegou ao primeiro mapa com a imagem certa, mas só pelo
Vulkan por software; no Switch nada foi testado ainda. O objetivo deste teste é saber até onde ele vai e trazer os
logs de volta.

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
