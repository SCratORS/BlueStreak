let btn = document.getElementsByClassName("collapse");
for (let i = 0; i < btn.length; i++) {
  btn[i].addEventListener("click", function () {
      let content = this.nextElementSibling;
      if (content.style.display == "block") {
          content.style.display = "none";
      } else {
          content.style.display = "block";
      }
  });    
}

var x, i, j, l, ll, selElmnt, a, b, c, selndx;
/* Look for any elements with the class "custom-select": */
x = document.getElementsByClassName("custom-select");
l = x.length;
for (i = 0; i < l; i++) {
  selElmnt = x[i].getElementsByTagName("select")[0];
  ll = selElmnt.length;
  /* For each element, create a new DIV that will act as the selected item: */
  a = document.createElement("DIV");
  a.setAttribute("class", "select-selected");
  a.innerHTML = selElmnt.options[selElmnt.selectedIndex].innerHTML;
  x[i].appendChild(a);
  /* For each element, create a new DIV that will contain the option list: */
  b = document.createElement("DIV");
  b.setAttribute("class", "select-items select-hide");
  for (j = 0; j < ll; j++) {
    /* For each option in the original select element,
    create a new DIV that will act as an option item: */
    c = document.createElement("DIV");
    c.innerHTML = selElmnt.options[j].innerHTML;
    c.addEventListener("click", function(e) {
        /* When an item is clicked, update the original select box,
        and the selected item: */
        var i, s, h, sl;
        s = this.parentNode.parentNode.getElementsByTagName("select")[0];
        sl = s.length;
        h = this.parentNode.previousSibling;
        for (i = 0; i < sl; i++) {
          if (s.options[i].innerHTML == this.innerHTML) {
            s.selectedIndex = i;
            sendCommand(s.getAttribute("action"), s.value);
            break;
          }
        }
        h.click();
    });
    b.appendChild(c);
  }
  x[i].appendChild(b);
  a.addEventListener("click", function(e) {
    /* When the select box is clicked, close any other select boxes,
    and open/close the current select box: */
    e.stopPropagation();
    closeAllSelect(this);
    this.nextSibling.classList.toggle("select-hide");
    this.classList.toggle("select-arrow-active");
  });
}

function closeAllSelect(elmnt) {
  /* A function that will close all select boxes in the document,
  except the current select box: */
  var x, y, i, xl, yl, arrNo = [];
  x = document.getElementsByClassName("select-items");
  y = document.getElementsByClassName("select-selected");
  xl = x.length;
  yl = y.length;
  for (i = 0; i < yl; i++) {
    if (elmnt == y[i]) {
      arrNo.push(i)
    } else {
      y[i].classList.remove("select-arrow-active");
    }
  }
  for (i = 0; i < xl; i++) {
    if (arrNo.indexOf(i)) {
      x[i].classList.add("select-hide");
    }
  }
}

/* If the user clicks anywhere outside the select box,
then close all select boxes: */
document.addEventListener("click", closeAllSelect);

var gateway = "ws://"+window.location.hostname+"/ws";
var websocket;
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
    sendCommand("getSettings",null);
}
function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}
function onMessage(event) {
    console.log(event.data);
    json = JSON.parse(event.data);
    for (var key in json) if (json.hasOwnProperty(key)) {
      if (key === 'alert') {
        alert(json[key]);
        return;
      }
      var element = document.getElementById(key);
      if (element) {
        if (element.nodeName.toLowerCase() === 'input') {
          if (element.type === 'text') element.value = json[key];
          if (element.type === 'password') element.value = json[key];
          if (element.type === 'number') element.value = json[key];
          if (element.type === 'checkbox') element.checked = json[key];
        }
        if (element.nodeName.toLowerCase() === 'div') {
          var element_value = element.querySelector("#value");
          if (element.getAttribute("type") === 'binary') {
            var element_icon = element.querySelector("#icon");
            if (element_icon) {
              if (json[key] == true) {
                element_icon.removeAttribute("class", "icon_binary-off");
                element_icon.setAttribute("class", "icon icon_binary-on");
                if (element_value) element_value.innerHTML = 'Включено';
              } else {
                element_icon.removeAttribute("class", "icon_binary-on");
                element_icon.setAttribute("class", "icon icon_binary-off");
                if (element_value) element_value.innerHTML = 'Выключено';
              }
            }
          } else if (element_value) element_value.innerHTML = json[key];
        }
        if (element.nodeName.toLowerCase() === 'select') {
          var s = element.parentNode.getElementsByClassName("select-selected")[0];
          s.innerHTML = element.options[json[key]].innerHTML;
          if (key === 'server_type') {
            let srv_1 = document.getElementById("mqtt_settings");
            let srv_2 = document.getElementById("tlg_settings");
            if (srv_1) srv_1.hidden = !(json[key] === 1);
            if (srv_2) srv_2.hidden = !(json[key] === 2);
          }
        }
        if (element.nodeName.toLowerCase() === 'audio') {
          var btn_play = document.getElementById(key+'_btn_play');
          var btn_delete = document.getElementById(key+'_btn_delete');
          if (btn_play) btn_play.hidden = json[key]==null;
          if (btn_delete) btn_delete.hidden = json[key]==null;
          if (json[key]) element.src=json[key];
          else element.src="";
        }
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

const BYTES_IN_MB = 1048576
let progressBarPanel = document.getElementById('update_firmware_progress');
let progressBar = progressBarPanel.querySelector("#progressBar");
let uploading = false;


function progressHandler(event) {
  // считаем размер загруженного и процент от полного размера
  const loadedMb = (event.loaded/BYTES_IN_MB).toFixed(1)
  const totalSizeMb = (event.total/BYTES_IN_MB).toFixed(1)
  const percentLoaded = Math.round((event.loaded / event.total) * 100)
  progressBar.value = percentLoaded
}

function loadHandler(event) {
  progressBar.value = 0;
  progressBarPanel.hidden = true;
  uploading = false;
}

function progressError(event) {
  progressBar.value = 0;
  progressBarPanel.hidden = true;
  uploading = false;
  alert("Ошибка загрузки");
}

function uploadMedia(element) {
  const file = element.files[0];
  if (uploading) alert("Дождитесь окончания предыдущей загузки.");
  if (!file) return;
  if (file.size > 4 * BYTES_IN_MB) {
    alert('Очень большой файл.')
    this.value = null
  } else {
    uploading = true;
    let progress = 'access_allowed_progress';
    if (element.id == 'access_allowed') progress = 'access_allowed_progress';
    if (element.id == 'delivery_allowed') progress = 'delivery_allowed_progress';
    if (element.id == 'access_denied') progress = 'access_denied_progress';

    progressBarPanel = document.getElementById(progress);
    progressBarPanel.hidden = false;
    progressBar = progressBarPanel.querySelector("#progressBar");

    const formSent = new FormData();
    const xhr = new XMLHttpRequest();
    formSent.append("media", file, element.id + ".mp3");
    xhr.upload.addEventListener('progress', progressHandler, false);
    xhr.upload.addEventListener('error', progressError, false);
    xhr.addEventListener('load', loadHandler, false);
    xhr.open('POST', '/upload');
    xhr.send(formSent);
  }
}

function updateOTA(element) {
  const file = element.files[0];
  if (uploading) alert("Дождитесь окончания предыдущей загузки.");
  if (!file) return;
  if (file.size > 4 * BYTES_IN_MB) {
    alert('Очень большой файл.')
    this.value = null
  } else {
    uploading = true;
    let progress = 'update_firmware_progress';
    progressBarPanel = document.getElementById(progress);
    progressBarPanel.hidden = false;
    progressBar = progressBarPanel.querySelector("#progressBar");

    const formSent = new FormData();
    const xhr = new XMLHttpRequest();
    formSent.append("firmware", file);
    xhr.upload.addEventListener('progress', progressHandler, false);
    xhr.upload.addEventListener('error', progressError, false);
    xhr.addEventListener('load', loadHandler, false);
    xhr.open('POST', '/update');
    xhr.send(formSent);
  }
}

