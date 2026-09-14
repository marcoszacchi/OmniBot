#pragma once
#include <pgmspace.h>

static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<title>OmniBot</title>
<style>
:root{--bg:#0f1115;--card:#1a1e26;--fg:#e8e8ea;--mut:#8b93a1;--acc:#4f8cff;--ok:#3ccf7a;--warn:#f5b342;--bad:#ff5a5a;--line:#2a3140}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent;user-select:none;-webkit-user-select:none}
html,body{margin:0;min-height:100%;background:var(--bg);color:var(--fg);font:15px/1.4 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;overscroll-behavior:none}
header{display:flex;align-items:center;justify-content:space-between;padding:10px 16px;background:var(--card);border-bottom:1px solid var(--line)}
header b{font-size:18px}
#st{font-size:13px;padding:4px 10px;border-radius:999px;background:var(--bad);color:#fff}
#st.ok{background:var(--ok);color:#062}
#st.cal{background:var(--warn);color:#000}
main{display:grid;grid-template-columns:1fr 1fr;gap:12px;padding:12px;max-width:1000px;margin:0 auto}
@media(max-width:700px){main{grid-template-columns:1fr}}
.card{background:var(--card);border-radius:14px;padding:12px}
h3{margin:0 0 8px;font-size:13px;color:var(--mut);text-transform:uppercase;letter-spacing:.05em}
#joy{position:relative;width:min(80vw,55vh,280px);height:min(80vw,55vh,280px);margin:0 auto;border-radius:50%;background:radial-gradient(circle,#262b36 0,#171a21 72%);border:2px solid var(--line);touch-action:none}
#joy:before,#joy:after{content:"";position:absolute;background:var(--line)}
#joy:before{left:50%;top:6%;width:1px;height:88%}
#joy:after{top:50%;left:6%;height:1px;width:88%}
#knob{position:absolute;left:33%;top:33%;width:34%;height:34%;border-radius:50%;background:var(--acc);box-shadow:0 6px 18px rgba(79,140,255,.45);pointer-events:none}
.rot{display:flex;align-items:center;gap:10px;margin-top:14px}
.rlab{font-size:26px;line-height:1;color:var(--mut);width:30px;text-align:center}
#rtrack{position:relative;flex:1;height:56px;border-radius:28px;background:#171a21;border:2px solid var(--line);touch-action:none}
#rtrack:before{content:"";position:absolute;left:50%;top:18%;width:1px;height:64%;background:var(--line)}
#rknob{position:absolute;top:4px;left:50%;width:44px;height:44px;margin-left:-22px;border-radius:50%;background:var(--acc);box-shadow:0 6px 18px rgba(79,140,255,.45);pointer-events:none}
.rhint{text-align:center;color:var(--mut);font-size:12px;margin-top:6px}
.armbtn{display:block;width:100%;padding:18px 10px;margin:0 0 12px;font-size:18px;font-weight:700;letter-spacing:.03em;border:0;border-radius:12px;background:var(--ok);color:#062}
.armbtn.off{background:var(--bad);color:#fff}
label.sl{display:block;margin:8px 0}
label.sl span{float:right;color:var(--mut)}
input[type=range]{width:100%;margin:6px 0 0;accent-color:var(--acc)}
.row{display:flex;gap:8px;flex-wrap:wrap;margin-top:8px}
.row button{flex:1;padding:12px 8px;border:0;border-radius:10px;background:#2a3140;color:var(--fg);font-size:14px}
.row button.warn{background:var(--warn);color:#000;font-weight:700}
.chk{display:flex;align-items:center;gap:8px;margin:8px 0}
.chk input{width:20px;height:20px;accent-color:var(--acc)}
.tele{display:grid;grid-template-columns:auto 1fr;gap:6px 12px;align-items:center;font-size:14px}
.bar{position:relative;height:12px;background:#0f1115;border-radius:6px;overflow:hidden}
.bar i{position:absolute;top:0;height:100%;background:var(--acc)}
.bar b{position:absolute;left:50%;top:0;width:1px;height:100%;background:var(--mut)}
.val{font-variant-numeric:tabular-nums}
.tests{display:grid;grid-template-columns:auto 1fr 1fr auto;gap:8px;align-items:center;margin-top:8px}
.tests button{padding:14px 0;font-size:18px;border:0;border-radius:10px;background:#2a3140;color:var(--fg);touch-action:none}
.tests button.on{background:var(--warn);color:#000}
.tests label{display:flex;align-items:center;gap:6px;font-size:13px;color:var(--mut);white-space:nowrap}
.tests input{width:20px;height:20px;margin:0;accent-color:var(--acc)}
.duty{display:flex;align-items:center;gap:8px;margin-top:12px}
.duty label{color:var(--mut);font-size:13px;white-space:nowrap}
.duty input{flex:1;min-width:0;padding:10px;border:1px solid var(--line);border-radius:10px;background:#0f1115;color:var(--fg);font-size:16px;user-select:text;-webkit-user-select:text}
.duty button{padding:12px 16px;border:0;border-radius:10px;background:var(--acc);color:#fff;font-weight:700}
.mut{color:var(--mut);font-size:13px;margin:6px 0}
</style>
</head>
<body>
<header><b>OmniBot</b><span id="st">desconectado</span></header>
<main>
 <section class="card">
  <h3>Movimento</h3>
  <div id="joy"><div id="knob"></div></div>
  <div class="rot">
   <span class="rlab">&#8634;</span>
   <div id="rtrack"><div id="rknob"></div></div>
   <span class="rlab">&#8635;</span>
  </div>
  <div class="rhint">Rota&ccedil;&atilde;o: arraste para a esquerda (anti-hor&aacute;rio) ou direita (hor&aacute;rio)</div>
 </section>
 <section class="card">
  <h3>Ajustes</h3>
  <button id="arm" class="armbtn">ARMADO</button>
  <label class="sl">Velocidade m&aacute;xima <span id="vmaxv">70%</span><input id="vmax" type="range" min="10" max="100" value="70"></label>
  <label class="sl">Velocidade de rota&ccedil;&atilde;o <span id="rotsv">50%</span><input id="rots" type="range" min="10" max="100" value="50"></label>
  <label class="chk"><input type="checkbox" id="hold" checked> Manter dire&ccedil;&atilde;o (girosc&oacute;pio)</label>
  <label class="chk"><input type="checkbox" id="field"> Controle orientado ao campo</label>
  <div class="row">
   <button id="zero">Zerar dire&ccedil;&atilde;o</button>
   <button id="cal">Calibrar b&uacute;ssola</button>
  </div>
  <div class="row">
   <button id="bt">Modo Bluetooth (controle Xbox)</button>
  </div>
 </section>
 <section class="card">
  <h3>Telemetria</h3>
  <div class="tele">
   <span>Dire&ccedil;&atilde;o</span><span class="val" id="yaw">--</span>
   <span>Giro</span><span class="val" id="gz">--</span>
   <span>B&uacute;ssola</span><span class="val" id="mag">--</span>
   <span>IMU</span><span id="imu">--</span>
   <span>Bot&atilde;o</span><span id="btn">--</span>
   <span>M1</span><div class="bar"><b></b><i id="b1"></i></div>
   <span>M2</span><div class="bar"><b></b><i id="b2"></i></div>
   <span>M3</span><div class="bar"><b></b><i id="b3"></i></div>
   <span>Comando</span><span class="val" id="cmd">[x 0.00 y 0.00 r 0.00]</span>
  </div>
 </section>
 <section class="card">
  <h3>Teste de motores (segure o bot&atilde;o)</h3>
  <p class="mut">Os bot&otilde;es mandam um sinal para os motores no n&iacute;vel escolhido. Com "+" a roda/eixo deve girar no sentido hor&aacute;rio, e com "-" no sentido anti-hor&aacute;rio. Caso isso n&atilde;o aconte&ccedil;a, inverta o sentido do motor: pela pr&oacute;pria p&aacute;gina, marcando "Inverter" ao lado dele, ou no c&oacute;digo, alterando a vari&aacute;vel MOTOR_INVERTED em config.h. O valor padr&atilde;o &eacute; (false, false, false).</p>
  <p class="mut">Para achar o sinal m&iacute;nimo de movimento dos motores, abaixe o n&iacute;vel at&eacute; o motor n&atilde;o girar quando pressionado um dos bot&otilde;es (+ ou -). Um valor acima desse &eacute; o n&uacute;mero que deve ser colocado em MOTOR_MIN_DUTY: pela pr&oacute;pria p&aacute;gina, no campo abaixo, ou no c&oacute;digo, alterando a vari&aacute;vel em config.h, lembrando que esse valor deve ser dividido por 100. O valor padr&atilde;o &eacute; 0.05 (5%).</p>
  <label class="sl">N&iacute;vel do teste <span id="tlvlv">40%</span><input id="tlvl" type="range" min="3" max="100" value="40"></label>
  <div class="tests">
   <span>M1</span><button data-m="1" data-v="-1">&minus;</button><button data-m="1" data-v="1">+</button><label><input type="checkbox" id="inv1"> Inverter</label>
   <span>M2</span><button data-m="2" data-v="-1">&minus;</button><button data-m="2" data-v="1">+</button><label><input type="checkbox" id="inv2"> Inverter</label>
   <span>M3</span><button data-m="3" data-v="-1">&minus;</button><button data-m="3" data-v="1">+</button><label><input type="checkbox" id="inv3"> Inverter</label>
  </div>
  <div class="duty">
   <label for="mduty">MOTOR_MIN_DUTY</label><input id="mduty" type="text" inputmode="decimal" autocomplete="off" value="0.05"><button id="mdutySave">Salvar</button>
  </div>
  <p class="mut">Teclado (no PC): W, A, S, D movem e Q/E rotacionam.</p>
 </section>
</main>
<script>
(function(){
var $=function(id){return document.getElementById(id);};
var st=$('st'),joy=$('joy'),knob=$('knob');
var ws=null,connected=false;
var jx=0,jy=0,jid=null,rot=0,kx=0,ky=0,krot=0;
var testM=0,testV=0,armed=true,calib=false;

function setStatus(){
  if(!connected){st.textContent='desconectado';st.className='';}
  else if(calib){st.textContent='calibrando bússola...';st.className='cal';}
  else{st.textContent='conectado';st.className='ok';}
}
function connect(){
  ws=new WebSocket('ws://'+location.hostname+':81/');
  ws.onopen=function(){connected=true;setStatus();send('H '+($('hold').checked?1:0));send('F '+($('field').checked?1:0));};
  ws.onclose=function(){connected=false;setStatus();setTimeout(connect,1000);};
  ws.onerror=function(){try{ws.close();}catch(e){}};
  ws.onmessage=function(e){tele(e.data);};
}
function send(s){if(connected&&ws&&ws.readyState===1)ws.send(s);}

/* ---------- joystick ---------- */
function joyPos(e){
  var r=joy.getBoundingClientRect(),R=r.width/2,lim=R*0.66;
  var dx=e.clientX-(r.left+R),dy=e.clientY-(r.top+R);
  var d=Math.hypot(dx,dy);
  if(d>lim){dx*=lim/d;dy*=lim/d;}
  knob.style.transform='translate('+dx+'px,'+dy+'px)';
  jx=dx/lim;jy=-dy/lim;
}
joy.addEventListener('pointerdown',function(e){if(jid!==null)return;jid=e.pointerId;joy.setPointerCapture(jid);joyPos(e);e.preventDefault();});
joy.addEventListener('pointermove',function(e){if(e.pointerId===jid)joyPos(e);});
function joyEnd(e){if(e.pointerId!==jid)return;jid=null;jx=0;jy=0;knob.style.transform='';}
joy.addEventListener('pointerup',joyEnd);
joy.addEventListener('pointercancel',joyEnd);

/* ---------- slider de rotação (volta ao centro ao soltar) ---------- */
var rtrack=$('rtrack'),rknob=$('rknob'),rid=null;
function rotPos(e){
  var r=rtrack.getBoundingClientRect(),cx=r.left+r.width/2,lim=r.width/2-26;
  var dx=e.clientX-cx;
  if(dx>lim)dx=lim;if(dx<-lim)dx=-lim;
  rknob.style.transform='translateX('+dx+'px)';
  rot=-dx/lim;   /* esquerda = anti-horário (+), direita = horário (-) */
}
rtrack.addEventListener('pointerdown',function(e){if(rid!==null)return;rid=e.pointerId;rtrack.setPointerCapture(rid);rotPos(e);e.preventDefault();});
rtrack.addEventListener('pointermove',function(e){if(e.pointerId===rid)rotPos(e);});
function rotEnd(e){if(e.pointerId!==rid)return;rid=null;rot=0;rknob.style.transform='';}
rtrack.addEventListener('pointerup',rotEnd);
rtrack.addEventListener('pointercancel',rotEnd);

/* ---------- botões de segurar (teste de motores) ---------- */
function hold(el,on,off){
  el.addEventListener('pointerdown',function(e){el.setPointerCapture(e.pointerId);el.classList.add('on');on();e.preventDefault();});
  var f=function(){if(el.classList.contains('on')){el.classList.remove('on');off();}};
  el.addEventListener('pointerup',f);el.addEventListener('pointercancel',f);el.addEventListener('lostpointercapture',f);
}
var tlvl=$('tlvl');
tlvl.oninput=function(){$('tlvlv').textContent=tlvl.value+'%';};
Array.prototype.forEach.call(document.querySelectorAll('.tests button'),function(b){
  hold(b,function(){testM=+b.getAttribute('data-m');testV=(+b.getAttribute('data-v'))*tlvl.value/100;send('R '+testM+' '+testV.toFixed(3));},
         function(){send('R '+testM+' 0');testM=0;testV=0;});
});

/* ---------- inversão dos motores e duty mínimo (salvos na flash do robô) ---------- */
var invBox=[null,$('inv1'),$('inv2'),$('inv3')];
for(var mi=1;mi<=3;mi++)(function(m){invBox[m].onchange=function(){send('I '+m+' '+(invBox[m].checked?1:0));};})(mi);
var mduty=$('mduty');
function saveMinDuty(){
  if(!connected)return;
  var v=parseFloat(mduty.value.replace(',','.'));
  if(isNaN(v)||v<0||v>0.8)return;
  send('D '+v.toFixed(2));mduty.blur();
}
$('mdutySave').onclick=saveMinDuty;
mduty.onkeydown=function(e){if(e.key==='Enter'){saveMinDuty();e.preventDefault();}};

/* ---------- teclado (teste no PC) ---------- */
var keys={};
function updKeys(){
  kx=(keys.d?1:0)-(keys.a?1:0);ky=(keys.w?1:0)-(keys.s?1:0);
  var m=Math.hypot(kx,ky);if(m>1){kx/=m;ky/=m;}
  krot=(keys.q?1:0)-(keys.e?1:0);
}
addEventListener('keydown',function(e){if(e.target.tagName==='INPUT')return;keys[e.key.toLowerCase()]=true;updKeys();});
addEventListener('keyup',function(e){keys[e.key.toLowerCase()]=false;updKeys();});
document.addEventListener('contextmenu',function(e){e.preventDefault();});

/* ---------- ajustes ---------- */
var vmax=$('vmax'),rots=$('rots');
vmax.oninput=function(){$('vmaxv').textContent=vmax.value+'%';};
rots.oninput=function(){$('rotsv').textContent=rots.value+'%';};
$('hold').onchange=function(e){send('H '+(e.target.checked?1:0));};
$('field').onchange=function(e){send('F '+(e.target.checked?1:0));};
$('zero').onclick=function(){send('Z');};

/* armar / desarmar (toggle) */
$('arm').onclick=function(){
  if(!connected)return;
  send('E '+(armed?0:1));
};

/* calibrar bússola: dois toques em 4 s (confirm() é bloqueado em alguns navegadores) */
var calBtn=$('cal'),calWait=false,calTimer=null;
function calReset(){calWait=false;calBtn.textContent='Calibrar bússola';calBtn.classList.remove('warn');}
calBtn.onclick=function(){
  if(!connected)return;
  if(!calWait){
    calWait=true;calBtn.textContent='Confirmar? (o robô vai girar ~12 s)';calBtn.classList.add('warn');
    calTimer=setTimeout(calReset,4000);return;
  }
  clearTimeout(calTimer);calReset();send('K');
};

/* modo Bluetooth: dois toques; o ESP desliga o WiFi (esta página para de funcionar) */
var btBtn=$('bt'),btWait=false,btTimer=null;
function btReset(){btWait=false;btBtn.textContent='Modo Bluetooth (controle Xbox)';btBtn.classList.remove('warn');}
btBtn.onclick=function(){
  if(!connected)return;
  if(!btWait){
    btWait=true;btBtn.textContent='Confirmar? (o WiFi desliga e esta página para)';btBtn.classList.add('warn');
    btTimer=setTimeout(btReset,4000);return;
  }
  clearTimeout(btTimer);btReset();send('B 1');
};

/* ---------- zona morta (na posição BRUTA, antes da escala de velocidade) ----------
   JOY_DZ: radial (centro). JOY_AX_DZ: "em cruz": perto de um eixo, a componente pequena
   vira zero, para andar 100% reto ou 100% de lado sem escorregar. */
var JOY_DZ=0.06,JOY_AX_DZ=0.05,ROT_DZ=0.06;
function dzRadial(x,y,d){var m=Math.hypot(x,y);if(m<d)return[0,0];var m2=Math.min(1,(m-d)/(1-d));return[x*m2/m,y*m2/m];}
function dz1(v,d){var a=Math.abs(v);if(a<d)return 0;var m=Math.min(1,(a-d)/(1-d));return v<0?-m:m;}
function dzCross(x,y,d){return[dz1(x,d),dz1(y,d)];}

/* ---------- envio periódico (20 Hz) ---------- */
setInterval(function(){
  if(!connected)return;
  var vm=vmax.value/100,rs=rots.value/100;
  var raw=dzRadial(jid!==null?jx:kx,jid!==null?jy:ky,JOY_DZ);
  raw=dzCross(raw[0],raw[1],JOY_AX_DZ);
  var x=raw[0]*vm,y=raw[1]*vm,r=dz1(rid!==null?rot:krot,ROT_DZ)*rs;
  if(testM)send('R '+testM+' '+testV.toFixed(3));
  send('C '+x.toFixed(3)+' '+y.toFixed(3)+' '+r.toFixed(3));
  $('cmd').textContent='[x '+x.toFixed(2)+' y '+y.toFixed(2)+' r '+r.toFixed(2)+']';
},50);

/* ---------- telemetria ---------- */
function bar(id,v){
  var el=$(id);v=Math.max(-1,Math.min(1,v));
  if(v>=0){el.style.left='50%';el.style.width=(v*50)+'%';}
  else{el.style.left=(50+v*50)+'%';el.style.width=(-v*50)+'%';}
  el.style.background=v>=0?'var(--acc)':'var(--warn)';
}
/* T yaw bussola m1 m2 m3 armado imuOk magOk hold campo calibrando clientes botao taxaZ biasZ ruidoZ parado giroValidado reiniciosMPU sentidoGiro sentidoConfirmado dutyMin inv1 inv2 inv3 */
function tele(s){
  var p=s.split(' ');if(p[0]!=='T')return;
  var imu=p[7]==='1',magok=p[8]==='1';
  var trusted=p[18]==='1';
  $('yaw').textContent=imu?p[1]+'°':'--';
  $('gz').textContent=imu?p[14]+' °/s':'--';
  $('mag').textContent=magok?p[2]+'°':'--';
  $('imu').textContent=!imu?'SEM RESPOSTA':(trusted?'OK':'AGUARDANDO');
  $('imu').style.color=!imu?'var(--bad)':(trusted?'var(--ok)':'var(--warn)');
  var btn=p[13]==='1';
  $('btn').textContent=btn?'1':'0';
  $('btn').style.color=btn?'var(--warn)':'var(--mut)';
  bar('b1',+p[3]);bar('b2',+p[4]);bar('b3',+p[5]);
  armed=p[6]==='1';
  $('arm').textContent=armed?'ARMADO':'DESARMADO';
  $('arm').className='armbtn'+(armed?'':' off');
  if(document.activeElement!==$('hold'))$('hold').checked=p[9]==='1';
  if(document.activeElement!==$('field'))$('field').checked=p[10]==='1';
  if(p.length>25){
    if(document.activeElement!==mduty)mduty.value=p[22];
    for(var i=1;i<=3;i++)if(document.activeElement!==invBox[i])invBox[i].checked=p[22+i]==='1';
  }
  calib=p[11]==='1';
  setStatus();
}
connect();
})();
</script>
</body>
</html>
)rawliteral";
