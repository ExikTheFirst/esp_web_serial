(() => {
  const $ = (sel) => document.querySelector(sel);
  const log = $('#log');
  const wsState = $('#wsState');

  function append(s){
    log.textContent += s;
    if (log.textContent.length > 300000) log.textContent = log.textContent.slice(-150000);
    log.scrollTop = log.scrollHeight;
  }

  function connectWS(){
    const scheme = location.protocol === 'https:' ? 'wss://' : 'ws://';
    const ws = new WebSocket(scheme + location.host + '/ws');
    ws.onopen = () => { wsState.textContent='připojeno'; wsState.classList.remove('bad'); wsState.classList.add('ok'); };
    ws.onclose = () => { wsState.textContent='odpojeno'; wsState.classList.add('bad'); wsState.classList.remove('ok'); setTimeout(connectWS, 1000); };
    ws.onerror = () => { try { ws.close(); } catch(_){} };
    ws.onmessage = (ev) => { append(ev.data); };
    window.__ws = ws;
  }

  window.addEventListener('DOMContentLoaded', () => {
    connectWS();

    $('#send').onclick = () => {
      const ws = window.__ws;
      if (!ws || ws.readyState !== 1) return;
      let msg = $('#tx').value;
      if ($('#crlf').checked) msg += "\r\n";
      ws.send(msg);
    };

    $('#clear').onclick = () => { log.textContent = ''; };

    $('#applyBaud').onclick = async () => {
      const b = $('#baud').value;
      try { await fetch('/setbaud?b=' + encodeURIComponent(b)); } catch(_) {}
    };
  });
})();
