//web.ino
#include <ESP8266WebServer.h>
ESP8266WebServer webServer(80);

const char PAGE_MAIN[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Гирлянда</title><style>
body{font-family:sans-serif;background:#111;color:#eee;max-width:430px;margin:0 auto;padding:12px}
button,select,input{font-size:16px;padding:8px;margin:4px 0;width:100%;box-sizing:border-box;border-radius:8px;border:none}
button{background:#2ecc71;color:#000;font-weight:bold}
button.off{background:#444;color:#ccc;font-weight:normal}
label{display:block;margin:10px 0 2px;color:#aaa;font-size:13px}
h2{color:#4fc3f7;margin:4px 0}
.row{display:flex;align-items:center;justify-content:space-between;gap:10px;margin:12px 0}
.row span{color:#aaa;font-size:14px}
input[type=number]{width:90px;margin:0;padding:6px;text-align:center}
.switch{position:relative;display:inline-block;width:48px;height:26px;flex-shrink:0}
.switch input{opacity:0;width:0;height:0;margin:0;padding:0}
.slider{position:absolute;cursor:pointer;inset:0;background:#555;border-radius:26px;transition:.2s}
.slider:before{content:"";position:absolute;height:20px;width:20px;left:3px;top:3px;background:#fff;border-radius:50%;transition:.2s}
input:checked+.slider{background:#2ecc71}
input:checked+.slider:before{transform:translateX(22px)}
</style></head><body>
<h2>🎄 Гирлянда</h2>
<button id="btnP" onclick="setP()">Питание</button>

<label>Яркость: <span id="vBr"></span></label>
<input type="range" min="0" max="255" id="br">

<label>Эффект</label>
<select id="type" onchange="send('type='+this.value)"></select>

<label>Палитра</label>
<select id="pal" onchange="send('pal='+this.value)"></select>

<button onclick="send('next=1')">Следующий эффект ⏭</button>
<button id="btnF" onclick="setFav()">Избранный</button>

<label>Масштаб: <span id="vSc"></span></label>
<input type="range" min="1" max="255" id="sc">
<label>Скорость: <span id="vSp"></span></label>
<input type="range" min="0" max="255" id="sp">

<div class="row"><span>Автосмена (эффекты + палитры)</span>
<label class="switch"><input type="checkbox" id="auto" onchange="send('auto='+(this.checked?1:0))"><span class="slider"></span></label></div>
<div class="row"><span>Автосмена палитр</span>
<label class="switch"><input type="checkbox" id="pauto" onchange="send('pauto='+(this.checked?1:0))"><span class="slider"></span></label></div>
<div class="row"><span>Случайный порядок</span>
<label class="switch"><input type="checkbox" id="rnd" onchange="send('rnd='+(this.checked?1:0))"><span class="slider"></span></label></div>
<div class="row"><span>Только избранные</span>
<label class="switch"><input type="checkbox" id="favonly" onchange="send('favOnly='+(this.checked?1:0))"><span class="slider"></span></label></div>

<div class="row"><span>Период смены: <span id="vPr"></span> мин</span>
<input type="number" min="1" max="255" id="pr" onchange="send('prd='+this.value)"></div>

<div class="row"><span>Таймер сна, мин (0 — выключен)</span>
<input type="number" min="0" max="255" id="ot" onchange="send('off='+this.value)"></div>

<script>
const TYPES=['Градиент','Перлин','Светлячки','Звёзды','Салют'];
const PALS=['Party','Радуга','Радуга-полосы','Закат','Синь-красный','Тёплый','Холодный','Огонь','Розовый','Киберпанк','Красно-белый','Лава','Океан','Лес','Облака','Аврора','Новый год','Угли'];
let S,selBuilt=false;
const $=id=>document.getElementById(id);

// записываем значение с сервера только если контрол не трогают сейчас
function g(id,v,lab){
  const el=$(id);
  if(document.activeElement===el||el.matches(':active'))return;
  el.value=v;if(lab)$(lab).textContent=v;
}

function bindSlider(id,lab,cmd){
  const el=$(id),sp=$(lab);
  el.addEventListener('input',()=>sp.textContent=el.value);
  el.addEventListener('change',()=>{sp.textContent=el.value;send(cmd+'='+el.value);});
}
bindSlider('br','vBr','br');
bindSlider('sc','vSc','sc');
bindSlider('sp','vSp','sp');

async function load(){
  try{
    S=await(await fetch('/api')).json();
    g('br',S.bright,'vBr');
    g('sc',S.curScale,'vSc');
    g('sp',S.curSpeed,'vSp');
    g('pr',S.prdCh,'vPr');
    g('ot',S.offTmr);
    $('auto').checked=!!S.autoCh;
    $('pauto').checked=!!S.palAuto;
    $('rnd').checked=!!S.rndCh;
    $('favonly').checked=!!S.favOnly;
    const ts=$('type'),ps=$('pal');
    if(!selBuilt){
      ts.innerHTML=TYPES.map((t,i)=>'<option value="'+i+'">'+t+'</option>').join('');
      ps.innerHTML=PALS.map((t,i)=>'<option value="'+i+'">'+t+'</option>').join('');
      selBuilt=true;
    }
    if(document.activeElement!==ts)ts.value=S.curType;
    if(document.activeElement!==ps)ps.value=S.curPal;
    btn($('btnP'),S.power?'Выключить':'Включить',S.power);
    btn($('btnF'),S.curFav?'★ Избранный':'☆ Не избранный',!S.curFav);
  }catch(e){}
}
function btn(b,txt,on){b.textContent=txt;b.className=on?'':'off';}
async function send(q){await fetch('/cmd?'+q);await load();}
function setP(){send('p='+(S.power?0:1));}
function setFav(){send('fav='+S.curEff);}
load();
setInterval(load,5000);
</script></body></html>)rawliteral";

byte curEffNow() {
  return forceTmr.state() ? forceEff : curEff;
}

void apiHandle() {
  byte ce = curEffNow();
  String s = "{\"ledAm\":" + String(cfg.ledAm);
  s += ",\"power\":" + String(cfg.power);
  s += ",\"bright\":" + String(cfg.bright);
  s += ",\"autoCh\":" + String(cfg.autoCh);
  s += ",\"palAuto\":" + String(cfg.palAuto);
  s += ",\"rndCh\":" + String(cfg.rndCh);
  s += ",\"favOnly\":" + String(cfg.favOnly);
  s += ",\"prdCh\":" + String(cfg.prdCh);
  s += ",\"turnOff\":" + String(cfg.turnOff);
  s += ",\"offTmr\":" + String(cfg.offTmr);
  s += ",\"palCount\":" + String(ACTIVE_PALETTES);
  s += ",\"effTotal\":5";
  s += ",\"curEff\":" + String(ce);
  s += ",\"curType\":" + String(ce / ACTIVE_PALETTES);
  s += ",\"curPal\":" + String(ce % ACTIVE_PALETTES);
  s += ",\"curFav\":" + String(effs[ce].fav);
  s += ",\"curScale\":" + String(effs[ce].scale);
  s += ",\"curSpeed\":" + String(effs[ce].speed);
  s += "}";
  webServer.send(200, "application/json", s);
}

void cmdHandle() {
  bool effCmd = webServer.hasArg("eff") || webServer.hasArg("fav") ||
                webServer.hasArg("sc") || webServer.hasArg("sp") ||
                webServer.hasArg("next") || webServer.hasArg("type") ||
                webServer.hasArg("pal");
  bool globalCmd = webServer.hasArg("br") || webServer.hasArg("p");  // глобальные настройки форс не трогают

  if (effCmd) {
    if (!forceTmr.state()) forceEff = curEff;  // форс истёк — актуализируем номер эффекта
    forceTmr.restart();
  } else if (!globalCmd) {
    if (forceTmr.state()) curEff = forceEff;
    forceTmr.stop();
  }

  if (webServer.hasArg("p")) {
    cfg.power = webServer.arg("p").toInt();
    if (!cfg.power) { stopFader(); strip->showLeds(0); }
  }
  if (webServer.hasArg("br")) cfg.bright = webServer.arg("br").toInt();
  if (webServer.hasArg("type")) {
    byte t = webServer.arg("type").toInt();
    if (t < 5) {
      forceEff = t * ACTIVE_PALETTES + curEffNow() % ACTIVE_PALETTES;
      curEff = forceEff;   // ← ДОБАВЛЕНО: показываем сразу, а не через 30 с
      stopFader();
    }
  }
  if (webServer.hasArg("pal")) {
    byte p = webServer.arg("pal").toInt();
    if (p < ACTIVE_PALETTES) {
      forceEff = (curEffNow() / ACTIVE_PALETTES) * ACTIVE_PALETTES + p;
      curEff = forceEff;   // ← ДОБАВЛЕНО
      stopFader();
    }
  }
  if (webServer.hasArg("next")) {
    byte t = curEffNow() / ACTIVE_PALETTES;
    forceEff = ((t + 1) % 5) * ACTIVE_PALETTES + curEffNow() % ACTIVE_PALETTES;
    curEff = forceEff;     // ← ДОБАВЛЕНО
    stopFader();
  }
  if (webServer.hasArg("fav")) {
    byte e = webServer.arg("fav").toInt();
    if (e < ACTIVE_PALETTES * 5) effs[e].fav = !effs[e].fav;
    EEeff.update();
  }
  if (webServer.hasArg("sc")) { effs[curEffNow()].scale = webServer.arg("sc").toInt(); EEeff.update(); }
  if (webServer.hasArg("sp")) { effs[curEffNow()].speed = webServer.arg("sp").toInt(); EEeff.update(); }
  if (webServer.hasArg("auto")) {
    cfg.autoCh = webServer.arg("auto").toInt();
    if (cfg.autoCh) { switchTmr.restart(); palSwitchTmr.stop(); }
    else if (cfg.palAuto) palSwitchTmr.restart();
    else palSwitchTmr.stop();
  }
  if (webServer.hasArg("pauto")) {
    cfg.palAuto = webServer.arg("pauto").toInt();
    if (cfg.palAuto && !cfg.autoCh) palSwitchTmr.restart();
    else palSwitchTmr.stop();
  }
  if (webServer.hasArg("rnd")) cfg.rndCh = webServer.arg("rnd").toInt();
  if (webServer.hasArg("favOnly")) {
    cfg.favOnly = webServer.arg("favOnly").toInt();
    // если включили, а текущий эффект не избранный — автосмена сама прыгнет
    // на избранный в свой срок; принудительно не дёргаем
  }
  if (webServer.hasArg("prd")) {
    cfg.prdCh = webServer.arg("prd").toInt();
    switchTmr.setPrd(cfg.prdCh * 60000ul);
    palSwitchTmr.setPrd(cfg.prdCh * 60000ul);
    if (cfg.autoCh) switchTmr.restart();
    else if (cfg.palAuto) palSwitchTmr.restart();
  }
  if (webServer.hasArg("off")) {
    byte m = webServer.arg("off").toInt();
    cfg.offTmr = m;
    cfg.turnOff = (m > 0);
    if (cfg.turnOff) {
      offTmr.setPrd(cfg.offTmr * 60000ul);
      offTmr.restart();
    } else offTmr.stop();
  }
  EEcfg.update();
  apiHandle();
}

void setupWeb() {
  webServer.on("/", []() { webServer.send_P(200, "text/html", PAGE_MAIN); });
  webServer.on("/api", apiHandle);
  webServer.on("/cmd", cmdHandle);
  webServer.begin();
}

void tickWeb() { webServer.handleClient(); }
