# Troféus (branch `trofeus`)

Sistema de troféus no console, sobre o que o SDK já tinha: a lista de conquistas do próprio jogo (nome, descrição,
ícone e gamerscore, lidos do XDBF do executável) e o registro dos desbloqueios no cartão
(`switch/ds2-nx/ds2/achievements/465307E4.toml`). O código está em `app/src/ds2_trophies.cpp`.

## O que há

- **Tela de troféus**: **L + Menos** abre e fecha (também L + R + Esquerda, o atalho do SDK; F7 no PC). Mostra o
  progresso (troféus e gamerscore), o cartão da platina e a lista: desbloqueados primeiro, do mais recente, com a
  data; os bloqueados em cinza com a dica do jogo. Direcional navega e rola; **B** ou **Menos** fecham. Enquanto ela
  está aberta o jogo não recebe o controle (regra do SDK para os seus menus).
- **Aviso de desbloqueio**: um cartão no canto inferior direito por 5 s (8 s para a platina). A janela se fecha
  sozinha quando não tem mais nada para mostrar: com qualquer janela ImGui aberta o apresentador do SDK pinta pela
  thread de interface, o caminho lento que derrubou o vigésimo build de ~20 para ~12 quadros/s depois de uma
  conquista; o aviso não pode ficar.
- **Platina nossa**, "Portador da Maldição": desbloqueia sozinha quando todos os troféus do jogo estão desbloqueados
  (os 1000G). Emblema desenhado no código (nenhuma imagem de terceiros), aviso maior. Fica no mesmo arquivo de
  desbloqueios, com o id `0x504C4154` ("PLAT"). Um perfil que já tinha tudo recebe a platina na primeira verificação.
- **Fonte**: a do próprio sistema do Switch (serviço `pl`, nada é distribuído); no PC, Segoe UI.

## Chaves (`ds2.toml`)

| chave | padrão | |
|---|---|---|
| `ds2_trophy_notices` | `true` | o aviso de desbloqueio |
| `ds2_trophy_platinum` | `true` | a platina |
| `ds2_trophy_test_at` | `0` | teste: nesse quadro apresentado mostra os avisos (platina e o primeiro troféu, sem desbloquear nada); 600 quadros depois abre a tela e 900 depois fecha |

## Conferido

No PC (`ds2_090`, `ds2_091`): a fonte carrega, os avisos e a tela abrem e fecham pelo teste, sem erro. A imagem não
pôde ser vista no PC (o driver de software não mostra a janela); o visual é conferido no console.
