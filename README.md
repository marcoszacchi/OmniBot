# OmniBot

Robô omnidirecional de três rodas defasadas 120°, controlado por um ESP32-C3 SuperMini.
Anda em qualquer direção sem virar o corpo, gira sobre o próprio eixo e mantém o rumo com o
giroscópio. Pode ser comandado por uma página web com joystick ou por um controle de
Xbox.

## Hardware

| Quantidade | Peça | Função |
|---|---|---|
| 1x | ESP32-C3 SuperMini | microcontrolador |
| 2x | DRV8833 | driver de motor CC (ponte H) |
| 3x | motor JGA25-370B CC 6 V 280 RPM | propulsão |
| 1x | MPU-6050 | giroscópio |
| 1x | QMC5883P | magnetômetro |
| 2x | célula 18650 3.7 V | alimentação |
| 1x | módulo para bateria 2S | carregador |
| 1x | regulador 5 V | alimentação do microcontrolador |
| 1x | botão | armar/desarmar, trocar modos de controle|
| 1x | LED | sinalização |

### Pinos (`config.h`)

| GPIO | Ligação |
|---|---|
| 0, 1 | IN1/IN2 - motor M1 |
| 3, 4 | IN3/IN4 - motor M2 |
| 5, 6 | IN1/IN2 - motor M3 |
| 7 | EEP |
| 8, 9 | SDA, SCL |
| 10 | LED |
| 20 | botão |


## Compilar e gravar

1. Arduino IDE com o pacote de placas **esp32** (testado com o core 2.0.14).
2. Placa **ESP32C3 Dev Module**, com **USB CDC On Boot: Enabled**.
3. Bibliotecas, pelo Gerenciador de Bibliotecas:

   | Biblioteca | Autor |
   |---|---|
   | WebSockets | Markus Sattler |
   | NimBLE-Arduino | h2zero |
   | XboxControllerNotificationParser | Asuki Kono |
   | MPU6050 | Electronic Cats |
   | Adafruit QMC5883P Library | Adafruit |

4. Abra `OmniBot.ino` e grave com o robô apoiado e parado: o giroscópio se calibra no boot.

Os comandos do protocolo também podem ser digitados no monitor serial.

## Como usar

### Botão e LED

| Botão | Efeito |
|---|---|
| toque curto | arma/desarma os motores |
| segurar 1,5 s | alterna entre modo WiFi e modo Bluetooth |

| LED | Significado |
|---|---|
| 1 piscada por segundo | modo WiFi |
| 2 piscadas por segundo | modo Bluetooth |
| aceso fixo | página ou controle conectado |
| pisca rápido | desarmado |
| pisca muito rápido | calibrando a bússola |
| 1 ou 2 piscadas no momento | um modo do controlador foi ligado ou desligado |

### Modo WiFi: página

Conecte na rede **OmniBot** (senha `omnibot_`) e abra `http://192.168.4.1/`. A página tem
joystick, slider de rotação, botão ARMADO/DESARMADO,
velocidade máxima e de rotação, "manter direção", "orientado ao campo", zerar direção,
calibrar bússola, trocar para Bluetooth, telemetria e um painel de teste de motores. No PC,
W A S D movem e Q/E giram.

### Modo Bluetooth: controle Xbox

Com o robô em modo
Bluetooth, segure o botão de parear do controle até o logo piscar rápido. Um controle já
pareado reconecta sozinho.

| Controle | Função |
|---|---|
| analógico esquerdo | translação |
| analógico direito | rotação |
| RT / LT | acelera até 100 % / reduz até 20 % |
| D-pad cima/baixo | velocidade máxima ±10 % |
| D-pad direita/esquerda | velocidade de rotação ±10 % |
| Menu | arma-desarma |
| View | zera a direção |
| Y | manter direção |
| X | orientado ao campo |
| B | para |
| LB + RB por 2 s | calibra a bússola |
| Menu + View por 2 s | volta ao modo WiFi |

O controle vibra para confirmar: 1 pulso = ligou/armou/conectou, 2 pulsos = desligou/
desarmou, pulso curto = velocidade alterada, 2 curtos = velocidade no limite.

### Primeira vez

1. No painel de teste da página, segure "+" de cada motor: a roda deve empurrar o robô no
   sentido anti-horário (visto de cima). Se um motor fizer o contrário, marque "Inverter"
   ao lado dele na página, ou `true` para ele em `MOTOR_INVERTED` no `config.h`.
2. Com as rodas no chão, abaixe o "Nível do teste" até o motor mal girar e coloque esse
   valor (dividido por 100) no campo `MOTOR_MIN_DUTY` da página e toque em "Salvar", ou na
   constante `MOTOR_MIN_DUTY` do `config.h`.

   O que for salvo pela página fica guardado na flash do robô e continua valendo depois de
   desligar e ligar. Só muda se outro valor for salvo pela página ou se a constante for
   alterada no `config.h` e o código for gravado de novo.
3. Gire o robô com o analógico por um segundo em cada sentido: o sentido do giroscópio é
   detectado e salvo sozinho (a linha "IMU" da telemetria mostra "confirmado").
4. Para testar no banco, sem rodas, desligue "manter direção".

## O código

| Arquivo | O que faz |
|---|---|
| `OmniBot.ino` | `setup()`, `loop()`, o laço de controle a 100 Hz (`controlTick`), botão, LED, telemetria e troca de modo |
| `config.h` | pinos e todos os parâmetros ajustáveis |
| `state.h` | `RobotState g_state`: o estado compartilhado (comando, armado, modos, pedidos, saída das rodas) |
| `motors.h/.cpp` | PWM de 20 kHz por LEDC nos DRV8833, em decaimento lento; duty mapeado entre mínimo e máximo; duty mínimo e inversão de cada motor salvos na flash |
| `kinematics.h/.cpp` | cinemática inversa: `w_i = −sin(θ_i)·vx + cos(θ_i)·vy + ω`, normalizada se alguma roda passar de 1 |
| `imu.h/.cpp` | MPU-6050: bias calibrado no boot e reajustado parado, integração do yaw, detecção do sentido do eixo Z; QMC5883P: rumo e calibração |
| `net.h/.cpp` | ponto de acesso WiFi, DNS cativo, HTTP, WebSocket e o interpretador de comandos |
| `web_page.h` | a página HTML/CSS/JavaScript |
| `xbox.h/.cpp` | cliente Bluetooth |

Fluxo: o rádio ativo (`net` ou `xbox`) escreve o comando em `g_state`; a cada 10 ms
`controlTick()` aplica o failsafe na rampa, a rotação para o
referencial do campo e o controlador PD do manter direção (`u = Kp·e − Kd·ω`,
com Kp = 0,02 por grau e Kd = 0,0015 por °/s, saturado em ±0,5), passa o resultado pela
cinemática inversa e escreve os motores. `imu::update()` lê o giro e a bússola.

### Protocolo (WebSocket na porta 81 ou monitor serial)

| Comando | Significado |
|---|---|
| `C x y r` | movimento: `x`, `y`, `r` de −1 a 1 (`y` + = frente, `r` + = anti-horário) |
| `S` | parada imediata |
| `E 0` / `E 1` | desarma / arma |
| `H 0` / `H 1` | manter direção |
| `F 0` / `F 1` | orientado ao campo |
| `Z` | zera a direção |
| `K` | calibra a bússola (o robô gira 12 s) |
| `T m v` | testa o motor `m` (1..3) com o comando `v` |
| `R m d` | testa o motor `m` com duty direto `d` |
| `B 0` / `B 1` | modo WiFi / Bluetooth |
| `X 0` | esquece o controle Xbox pareado |
| `D v` | salva o duty mínimo `v` (0 a 0,8) na flash |
| `I m 0` / `I m 1` | motor `m` (1..3) normal / invertido, salvo na flash |

Telemetria, 10 vezes por segundo:

```text
T yaw bussola m1 m2 m3 armado imuOk magOk hold campo calibrando clientes botao taxaZ biasZ ruidoZ parado giroValidado reiniciosMPU sentidoGiro sentidoConfirmado dutyMin inv1 inv2 inv3
```

## Ajustes em `config.h`

| Constante | Padrão | Efeito |
|---|---|---|
| `MOTOR_MAX_DUTY` | 0,80 | teto do PWM (motores de 6 V em bateria 2S) |
| `MOTOR_MIN_DUTY` | 0,05 | duty em que o motor começa a girar (padrão; pode ser salvo pela página) |
| `MOTOR_INVERTED` | false ×3 | inverte o sentido de cada motor (padrão; pode ser salvo pela página) |
| `INPUT_SLEW_PER_S` | 5,0 | rampa das entradas (0 a 100 % em 200 ms) |
| `COMMAND_TIMEOUT_MS` | 400 | failsafe |
| `HEADING_KP`, `HEADING_KD` | 0,02; 0,0015 | ganhos do manter direção |
| `HEADING_MAX_CORRECTION` | 0,5 | correção máxima do manter direção |
| `HEADING_DEADBAND_DEG` | 1,5 | zona morta do erro de rumo |
| `XBOX_STICK_DEADZONE` | 0,22 | zona morta dos analógicos do controle |
| `AP_SSID`, `AP_PASSWORD` | OmniBot, omnibot_ | rede WiFi |
| `PWM_FREQ_HZ` | 20000 | frequência do PWM |

IMPORTANTE: Para calibrar os motores utilize a página web e siga o passo a passo explicado na seção.