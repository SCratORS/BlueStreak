const uri = 'wss://uniproxy.alice.yandex.net/uni.ws';
var websocket;
window.addEventListener('load', onLoad);
function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(uri);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}
function onOpen(event) {
    console.log('Connection opened');
}
function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

function onMessage(event) {
    console.log(typeof(event.data));
    if (typeof(event.data) === 'object') {
	    audio_data.push(event.data.slice(4));
	    audio_received = true;
	} else if (typeof(event.data) === 'string') {
		json = JSON.parse(event.data);
		for (var key in json) if (json.hasOwnProperty(key)) {
	      if (key === 'directive') {
	        if (json['directive']['header']['name'] == 'Speak') {
	        	stream_id = json['directive']['header']['streamId'];
	        	audio_data = [];
                stream_active = true;
                audio_received = false;
	        }
	      } else if (key === 'streamcontrol') {
	      	if (json['streamcontrol']['streamId'] == stream_id) {
	      		let action = json['streamcontrol']['action'];
	      		if (action === 0 && audio_received) {
	      			var blob = new Blob(audio_data, {type: "media/mp3"});
					var link = document.createElement('a');
					link.href = window.URL.createObjectURL(blob);
					var fileName = "alice_tts_" + Date.now() + ".mp3";
					link.download = fileName;
					link.click();
	      		}
	      	}
	      }
	  	}
	}
}
function tts(msg) {
  	let message = {
	    event: {
	        header: {
	            messageId: "e9355a7f-6c86-4a49-8c85-2d80532bca90",
	            name: "Generate",
	            namespace: "TTS"
	        },
	        payload: {
	            format: "audio/mp3",
	            text: msg,
	            voice: "shitova.us"
	        }
	    }
	};
  let json = JSON.stringify(message);
  websocket.send(json);
}
function onLoad(event) {
    initWebSocket();
}
