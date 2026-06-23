var slidersEventInhibit = false;
var battTimeoutId = -1;

var bodyElement = document.getElementsByTagName('body')[0];
var statusElement = document.getElementById('status');
var disableAdcElement = document.getElementById('DisableADC');
var cutLevelsElement = document.getElementById('CutLevels');
var startRampElement = document.getElementById('startRamp');
var levelAElement = document.getElementById('LevelA');
var levelBElement = document.getElementById('LevelB');
var multiAdjustElement = document.getElementById('MultiAdjust');
var levelAIndicatorElement = document.getElementById('LevelAIndicator');
var levelBIndicatorElement = document.getElementById('LevelBIndicator');
var multiAdjustIndicatorElement = document.getElementById('MultiAdjustIndicator');
var modeButtonsElements = document.getElementsByClassName('modeButton');

levelAElement.eventTime = 0;
levelAElement.addEventListener("input", (event) => {
	event.target.eventTime = Date.now()
	levels(levelAElement, event.target.value)
});

levelBElement.eventTime = 0;
levelBElement.addEventListener("input", (event) => {
	event.target.eventTime = Date.now()
	levels(levelBElement, event.target.value)
});

multiAdjustElement.eventTime = 0;
multiAdjustElement.addEventListener("input", (event) => {
	event.target.eventTime = Date.now()
	levels(multiAdjustElement, event.target.value)
});

var ws;
initiateWebSocketConnection();
function initiateWebSocketConnection() {
	if (window.location.hash != "") {
		hostname = window.location.hash.substr(1); // for debugging purposes
	}
	else if (window.location.hostname != "") {
		hostname = window.location.hostname;
	}
	else {
		hostname = prompt("Please the IP address of your MK312Wifi");
		window.location = window.location+'#'+hostname;
	}

	try {
		ws = new WebSocket('ws://' + hostname + ':81/');
		statusElement.innerHTML+= "Connecting to "+hostname+"... ";
	}
	catch (err) {
		statusElement.innerHTML = err;
		if (window.location.href.startsWith("https://")) {
			window.location = window.location.href.replace("https://", "http://");
		}
		ws = null;
		return;
	}

	ws.onclose = function () {
		statusElement.innerHTML = "Connection closed. "
		setTimeout(initiateWebSocketConnection, 1000);
	}

	ws.showConnErrMsg = function () {
		statusElement.innerHTML = "Connection error! ";
	}

	ws.onerror = function () {
		setTimeout(ws.showConnErrMsg, 50);
		bodyElement.style.background = '#ff0000';
	}

	ws.onopen = function () {
		bodyElement.style.background = 'black';
		statusElement.innerHTML = "Connected! ";
		setTimeout(clearStatus, 1000);
	}

	ws.onmessage = (event) => {
		const resp = event.data.toString();
		console.log("<== " + resp);
		parseBoxResponse(resp);
	}
}

function clearStatus() {
	statusElement.innerHTML = "&nbsp;";
}

function wsSend(msg) {
	console.log("==> " + msg);
	if (ws === null) {
		return false;
	}
	if (!ws.readyState) {
		statusElement.innerHTML = "Message not sent: connection not ready!";
		return false;
	}
	ws.send(msg);
	return true;
}

function parseBoxResponse(msg) {
	if (msg.endsWith(' ERR')) {
		statusElement.innerHTML = msg;
		return;
	}

	slidersEventInhibit = true;
	for (var s of msg.split(' ')) {
		a = s.split("=");
		switch (a[0]) {
			case 'LevelA':
				val = parseInt(a[1]);
				levels(LevelA, val);
				levelAIndicatorElement.innerHTML = val + ' (' + byteToPercent(val)+'%)';
				levelAIndicator2.innerHTML = byteToPercent(val);
				cutLevelsElement.style.background = cutLevelsElement.state?'#a00000':'';
				if (cutLevelsElement.state && msg.endsWith(' OK')) {
					levelBElement.value = 0;
					levelBIndicator2.innerHTML = '0';
					cutLevelsElement.state = false;
					cutLevelsElement.style.background = '';
				}
				break
			case 'LevelB':
				val = parseInt(a[1]);
				levels(LevelB, val);
				levelBIndicatorElement.innerHTML = val + ' (' + byteToPercent(val)+'%)';
				levelBIndicator2.innerHTML = byteToPercent(val);
				if (cutLevelsElement.state && msg.endsWith(' OK')) {
					levelAElement.value = 0;
					levelAIndicator2.innerHTML = '0';
					cutLevelsElement.state = false;
					cutLevelsElement.style.background = '';
				}
				break
			case 'MultiAdjust':
				val = parseInt(a[1]);
				levels(MultiAdjust, val);
				multiAdjustIndicatorElement.innerHTML = val+'%';
				break
			case 'CutLevels':
				val = parseInt(a[1]);
				if (val) {
					levelAIndicatorElement.innerHTML = '0%';
					levelBIndicatorElement.innerHTML = '0%';
					cutLevelsElement.state = true;
					cutLevelsElement.style.background = '#a00000';
				}
				else {
					cutLevelsElement.state = false;
					cutLevelsElement.style.background = '';
					disableAdcElement.state = false;
					disableAdcElement.style.background = '';
				}
				break;
			case 'DisableADC':
				val = parseInt(a[1]);
				if (!cutLevelsElement.state || msg.endsWith(' OK') || !val) {
					disableAdcElement.state = val?true:false;
					disableAdcElement.style.background = val?'#00a000':'';
					cutLevelsElement.state = false;
					cutLevelsElement.style.background = '';
				}
				break;
			case 'startRamp':
				const myTimeout = setTimeout(function(button) {startRampElement.style.background = '';}, 2000, this);
				startRampElement.style.background = '#00a000';
				break;
			case 'Mode':
				for(var g = 0; g < modeButtonsElements.length; g++) {
					if (a[1] == modeButtonsElements[g].getAttribute('name')) { b=true; }
					else if (a[1] == modeButtonsElements[g].innerHTML) { b=true; }
					else { b=false; }

					modeButtonsElements[g].style.background = b?'#00a000':'';
				}
				break;
			case 'Range':
				range = parseInt(a[1]);
				console.log("Power Level Range:", range);
				switch (range) {
					case 1:
						levelRange.innerHTML = 'LOW';
						levelRange.style.background = '#0080ff';
						break;
					case 2:
						levelRange.innerHTML = 'MED';
						levelRange.style.background = '';
						break;
					case 3:
						levelRange.innerHTML = 'HIGH';
						levelRange.style.background = '#ff8000';
						break;
				}
				break;
			case 'Batt':
				batteryLevelRaw = parseInt(a[1]);;
				batteryVoltage = batteryLevelRaw / 12.425;
				batteryPercent = Math.round((batteryLevelRaw-143) * 4.67);
				if (batteryPercent > 100) { batteryPercent=100; }
				else if (batteryPercent < 0) {batteryPercent=0; }
				console.log("Battery: "+batteryPercent + "% ("+(batteryVoltage).toFixed(2), "V)");
				batteryText.innerHTML = batteryVoltage.toFixed(1)+'V';
				if      (batteryPercent > 30) { battery.style.background = ''; }
				else if (batteryPercent > 20) { battery.style.background = '#ff9000'; }
				else {                          battery.style.background = '#ff2000'; }
				batteryFill.style.width = batteryPercent+'%';
				battery.style.display = 'inline-block';

				clearTimeout(battTimeoutId);
				battTimeoutId = setTimeout(() => {
					wsSend("Batt?");
				}, 20000+Math.floor(10000*Math.random()));

				break;
			case 'Ver':
				console.log("MK3132Wifi firmware version:", a[1]);
			case 'OK':
				// yheee!
				break
			default:
				statusElement.innerHTML = msg;
		}
	}
	slidersEventInhibit = false;
}

function byteToPercent(val) {
	if (val == 255) {
		return 100;
	}
	return Math.floor(val/2.56);
}

function exec(obj, val) {
	const cmd = obj.id + (typeof(val) == 'undefined' ?"":"=" + val);
	return wsSend(cmd);
}

function levels(obj, val) {
	if (val > 255) { val=255; }
	else if (val < 0) { val=0; }

	if (!slidersEventInhibit) {
		if (!DisableADC.state) {
			wsSend("DisableADC=1")
			disableAdcElement.state = true;
		}
		exec(obj, val);
	}
	else {
		if (Date.now() - obj.eventTime > 1000) {
			obj.value = val;
		}
	}
	return val;
}

buttonsClickHandler();
function buttonsClickHandler() {
	for(var x = 0; x < modeButtonsElements.length; x++) {
		modeButtonsElements[x].onclick = function() {
			var send = 'Mode=' + this.getAttribute('name');
			wsSend(send);
		}
	}

	var buttons = document.getElementsByClassName('clickButton');
	for(var x = 0; x < buttons.length; x++) {
		buttons[x].onclick = function() {
			send = this.getAttribute('name');
			wsSend(send);
		}
	}

	var buttons = document.getElementsByClassName('toggleButton');
	for(var x = 0; x < buttons.length; x++) {
		buttons[x].state = false;
	}

	var buttons = document.getElementsByClassName('levelButton');
	this.timeoutId = null;
	for(var x = 0; x < buttons.length; x++) {
		buttons[x].onclick = function() {
			increment = parseInt(levelIncrement.value);
			switch (this.id) {
				case 'decrementA':
					element = levelAElement;
					increment*= -1;
					break;
				case 'incrementA':
					element = levelAElement;
					break;
				case 'decrementB':
					element = levelBElement;
					increment*= -1;
					break;
				case 'incrementB':
					element = levelBElement;
					break;
				default:
					return;
			}
			oldval = parseInt(element.value);
			newval = levels(element, oldval+increment);
			if (increment < 0) { // if for any reason there is a delay in connection, avoid multiples positives increments to build-up
				element.value = newval;
			}
			if (newval != oldval) {
				this.style.background = '#4040ff';
				clearTimeout(this.timeoutId);
				this.timeoutId = setTimeout(() => {
					this.style.background = '';
					this.timeoutId = null;
				}, 100);
				try { navigator.vibrate(15); }
				catch { } // not handled by the navigator?
			}
		}
	}

	disableAdcElement.onclick = function() {
		this.state = !this.state;
		msg = 'DisableADC=' + (this.state?'1':'0')

		if (this.state == true) {
			wsSend(msg);
			wsSend("LevelA="+levelAElement.value);
			wsSend("LevelB="+levelBElement.value);
		}
		else {
			wsSend(msg);
		}
		cutLevelsElement.state = false;
		cutLevelsElement.style.background = '';
	}

	cutLevelsElement.onclick = function() {
		this.state = !this.state;
		msg = 'CutLevels=' + (this.state?'1':'0')

		if (this.state == false) {
			if (disableAdcElement.state == true) {
				wsSend("LevelA="+levelAElement.value);
				wsSend("LevelB="+levelBElement.value);
			}
			else {
				wsSend(msg);
			}
		}
		else {
			wsSend(msg);
		}
	}

	showLevelsButtons.onclick = function() {
		this.state = !this.state;
		this.style.background = this.state?'#00a000':'';

		if (this.state) {
			levelsButtonsBlock.style.display = 'block';
			levelsSlidersBlock.style.display = 'none';
		}
		else {
			levelsButtonsBlock.style.display = 'none';
			levelsSlidersBlock.style.display = 'block';
		}
	}
}

updateLevelIncrementLabel();
function updateLevelIncrementLabel() {
		levelIncrementIndicator.innerHTML = levelIncrement.value + ' (~'+byteToPercent(levelIncrement.value)+'%)';
}
levelIncrement.addEventListener("input", (event) => {
	updateLevelIncrementLabel();
});

function displayHelp() {
	help.style.display = 'block';
	interrog.style.display = 'none';
}

function closeHelp() {
	help.style.display = 'none';
	interrog.style.display = 'inline';
}
