const protocol = (window.location.protocol === 'https:') ? 'wss://' : 'ws://';
const gateway = protocol + window.location.host + '/ws';
var websocket;
var element = document.getElementById("web_log");
window.addEventListener('load', onLoad);
function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    if (!window.location.hostname) {
      console.log('Hostname is undefined. Return.');
      return;
    }
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}
function onOpen(event) {
    console.log('Connection opened');
    sendCommand("enableLog", true);
}
function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}
function onMessage(event) {
  console.log(event.data);
  if (!element) return;
  json = JSON.parse(event.data);
  for (var key in json) if (json.hasOwnProperty(key)) {
    if (key === 'text') {
      element.value += '[' + Date.now() + '] ' + json[key];
    }
  }
}
function onLoad(event) {
    initWebSocket();
}
function sendCommand(action, msg) {
  let message = {
    method: action,
    value: msg
  };
  let json = JSON.stringify(message);
  websocket.send(json);
}