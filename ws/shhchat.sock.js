/* 
* shhchat web socket library
* No dependencies required (apart from a modern browser)
* Created by Chris Andrews
*/

window.shhchatWebSocket = (function() {
	window.Websocket = window.WebSocket || window.MozWebSocket;
	var ws = null;

	var chatBox, chatInput, chatStatus, chatSend, chatConn, chatDis;

	var shhchatHandler = {
		start: function() {
			chatBox = document.getElementById('shhchat-box');
			chatInput = document.getElementById('shhchat-input');
			chatStatus = document.getElementById('shhchat-status');
			chatSend = document.getElementById('shhchat-send');
			chatConn = document.getElementById('shhchat-connect');
			chatDis = document.getElementById('shhchat-disconnect');

			chatSend.onclick = function () {
				if (ws === null) {
					alert('You are disconnected');
				} else {
					ws.send(chatInput.value);
					chatBox.value = chatBox.value + '\n' + chatInput.value;
					chatInput.value = '';
					// Autoscroll to bottom
					chatBox.scrollTop = chatBox.scrollHeight;
				}
			};

			chatConn.onclick = function () {
				ws = new WebSocket('ws://localhost:9957', 'shhchat-protocol');
				ws.onopen = function () {
					chatStatus.style.color = 'green';
					chatStatus.innerHTML = 'Connected';
				};

				ws.onerror = function () {
					chatStatus.style.color = 'red';
					chatStatus.innerHTML = 'Disconnected';
				};

				ws.onclose = function () {
					chatStatus.style.color = 'red';
					chatStatus.innerHTML = 'Disconnected';
				};

				ws.onmessage = function (data) {
					console.log(data);
					// chatBox.value = chatBox.value + '\n' + data;
				};
			};

			chatDis.onclick = function () {
				if (ws !== null) {
					ws.close();
					ws = null;
				}
			};

		}
    	};

	return shhchatHandler;
}());


