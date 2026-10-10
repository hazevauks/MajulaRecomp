# Testando o Dark Souls II (MajulaRecomp) no Switch

Este é o **primeiro build para o console**. No PC o jogo chegou ao primeiro mapa com a imagem certa, mas só pelo
Vulkan por software; no Switch nada foi testado ainda. O objetivo deste teste é saber até onde ele vai e trazer os
logs de volta.

## O que vai no cartão SD

Copie a pasta `ds2-nx` inteira para `sdmc:/switch/` (fica `sdmc:/switch/ds2-nx/`):

```
switch/ds2-nx/ds2.nro                  o jogo
switch/ds2-nx/ds2.toml                 configuração (log ligado nestes testes)
switch/ds2-nx/ds2_shaders.mesp         biblioteca de shaders do renderizador nativo
switch/ds2-nx/ds2_shaders.mesp.idx
switch/ds2-nx/game_root.nxpack         os arquivos do jogo num arquivo só (≈ 5,8 GB)
switch/ds2-nx/game_root.nxidx          índice do pacote
switch/ds2-nx/configs/                 configurações alternativas (ver abaixo)
```

O cartão precisa estar em **exFAT ou FAT32 com suporte a arquivos grandes**: o `game_root.nxpack` passa de 4 GB. Em
FAT32 puro ele não cabe; nesse caso me avise, que eu monto o pacote com a pasta `game_root/` solta (o console monta o
pacote no primeiro boot).

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
- `ds2_melhorias_desligadas.toml`: desliga todos os atalhos de CPU e GPU herdados dos outros portes. Se o jogo travar
  ou a imagem sair errada com a normal, tente esta e me diga se mudou.
- `ds2_gpu_emulada.toml`: usa a GPU emulada no lugar do renderizador nativo. É lentíssima no console; só serve para
  comparar a imagem numa tela parada.
