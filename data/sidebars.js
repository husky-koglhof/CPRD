/* global bootstrap: false */

var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
var jsonobjs = {};
let cbx;
let dev = false;

var actionArray = {
  "settings_save": 0,
  "ping": 1,
  "reboot": 2,
  "reset": 3,
  "relay_on": 4,
  "relay_off": 5,
  "servo_pulse": 6,
  "cancel": 7,

  "pulse_start": 10,
  "pulse_stop": 11,
  "pulse_save": 12,

  "cycle_start": 13,
  "cycle_stop": 14,
  "cycle_save": 15,

  "lung_start": 16,
  "lung_stop": 17,
  "lung_save": 18,

  "thermometer_start": 19,
  "thermometer_stop": 20,
  "thermometer_save": 21,
  "pulse_activate": 22,
};

window.addEventListener('load', onLoad);

function initWebSocket() {
  console.log('Trying to open a WebSocket connection...');
  websocket = new WebSocket(gateway);
  websocket.onopen    = onOpen;
  websocket.onclose   = onClose;
  websocket.onmessage = onMessage; // <-- add this line
}
function onOpen(event) {
  console.log('Connection opened');
}

function onClose(event) {
  console.log('Connection closed');
  if (location.origin !== "http://localhost") {
    setTimeout(initWebSocket, 2000);
  }
}

function onMessage(e) {
  var nodes = JSON.parse(e.data);
  var color = "ok";

  if ($('a[data-address=' + nodes.id + ']').length == 0) {
    // TODO: we need actual saved data for each element
    window.nodes[nodes.id] = nodes;
    window.nodes[nodes.id].timer = Date.now();

    var masterStyle = nodes.m ? '<span style="position: fixed; margin-left: -27px; margin-top: 7px;opacity: 0.5;">m</span>' : "";
    var html = '<li><a data-page="node" data-address=' + nodes.id + ' href="#" class="own-link nav-link py-3 border-bottom" data-bs-toggle="tooltip" data-bs-placement="right">';
    html += '<svg class="bi" width="24" height="24" role="img" aria-label="' + nodes.id + '"><use xlink:href="#heartbeat"/></svg>';
    var volt = nodes.v; // voltage

    if (volt < 80) {
      color = "danger";
    } else if (volt < 90) {
      color = "warning";
    } else {
      color = "ok";
    }
    html += '<span class="position-absolute top-1 start-1 translate-middle p-1 border border-light rounded-circle bg-' + color + '">';
    html += '<span class="visually-hidden">New alerts</span>';
    html += masterStyle;
    html += '</span>';

    html += '</a></li>';

    $('#treeit').append(html);

    tinysort('ul#treeit>li', {selector:'a',attr:'data-address'});
  } else {
    window.nodes[nodes.id] = nodes;
    window.nodes[nodes.id].timer = Date.now();
    if (boardID == nodes.id) {
      renderLiveData();

      /*  TODO: Not working
      // change values on person if socket data changed
      let value = nodes.pID;

      if ($('.params[data-id="person"]').val() == value) {
        if (parseInt($('.params[data-id="pulseStrength"]').val()) != nodes.P[value].s ||
          parseInt($('.params[data-id="pulseTimeGap"]').val()) != nodes.P[value].t ||
          parseInt($('.params[data-id="pulseFrequence"]').val()) != nodes.P[value].f ||
          parseInt($('.params[data-id="pulseDuration"]').val()) != nodes.P[value].d ||
          parseInt($('.params[data-id="pulseFlicker"]').val()) != nodes.P[value].r) {
            console.log("Data changed");
            $('#person').val(value).change();
          }
      }
      */
      /*
      if ($('.params[data-id="pumpStart"]').length > 0) {
        if (parseInt($('.params[data-id="pumpStart"]').val()) !== nodes.B.s || 
          parseInt($('.params[data-id="pumpDuration"]').val()) !== nodes.B.d) {
            $('.nav-link').click();
        }
      }

      if ($('.params[data-id="thermometerDuration"]').length > 0) {
        if (parseInt($('.params[data-id="thermometerDuration"]').val()) !== nodes.T.d) {
          $('.nav-link').click();
        }
      }

      if ($('.params[data-id="lungStart"]').length > 0) {
        if (parseInt($('.params[data-id="lungStart"]').val()) !== nodes.L.s ||
          parseInt($('.params[data-id="lungDuration"]').val()) !== nodes.L.d) {
            $('.nav-link').click();
        }
      }
    */
    }
    if (nodes.pi) {
      if ($('[data-address="' + nodes.id + '"][data-page="node"]')[0].classList.contains("blink_me")) {
        // Do nothing, already blink
      } else {
        $('[data-address="' + nodes.id + '"][data-page="node"]').addClass('blink_me');
      }
    } else {
      $('[data-address="' + nodes.id + '"][data-page="node"]').removeClass('blink_me');
    }
  }
}

window.boardID = 0;
window.nodes = {};

function loadFile(obj, page) {
  $.ajax({
    url: page,
    type: "GET",
    cache: false,
    success: function (html) {
  
     /* Replace Content */
     obj.html(html);
    }
   });
}
$(document).ready(function () {
  setTimeout(function() {
    loadFile($('#modalDiv'), "/modals.htm");
  }, 500);

  $('#spinning').modal('show');
  
  loadFile($('#dynamiccontent'), "/home.htm");

  $('.settingsButton').on("click", function(event) {
    $('#spinning').modal('show');
    $('#dynamiccontent').empty();
    loadFile($('#dynamiccontent'), "/settings");
    $('.own-link').removeClass('active');
    hideModal(500);
  });
  
  $('.updateButton').on("click", function(event) {
    $('#spinning').modal('show');
    hideModal(2000);

    $('.own-link').removeClass('active');
    $('#dynamiccontent').empty();
    $("#dynamiccontent").append('<iframe class="my-iframe" src="/update" frameborder="0" style="border:0;overflow:hidden;height:90%%;width:100%%;" allowfullscreen height="90%%" width="100%%"></iframe>');
  });

  $('.pingButton').on("click", function() {
    $('[data-page="node"].active').addClass('blink_me');
    sendDataHome("ping", ["boardID", parseInt($('#boardID').val())]);
  });

  $(document).on("click", '[data-class="yesNo"]', function(event) {
    let parent = $(this).parent().parent().parent().parent()[0].id;

    if ($(this).data("id") == "Yes") {
      if (parent == "yesNoReset") {
        sendDataHome("reset", [], 10000);
      } else if (parent == "yesNoReboot") {
        sendDataHome("reboot", ["boardID", parseInt($('#boardID').val())]);
      }
      setTimeout(function() {
        $('#dynamiccontent').empty();
        location.reload();
      }, 5000);
    }
  });

  $(document).on("change", "[data-id='checkSSID']", function(event) {
    if ($(this)[0].checked == true) {
      $("[data-id='ownSSID']").removeAttr("disabled");
      $("[data-id='ownSSID']").val("CPRD_" + boardID);
    } else {
      $("[data-id='ownSSID']").prop("disabled", true);
      $("[data-id='ownSSID']").val("CPRD");
    }
  });

  $(document).on('click', '.own-link', function(event) {
    $('#spinning').modal('show');
    $('#dynamiccontent').empty();
    var address = $(this).data("address");
    let page = $(this).data("page");
    window.boardID = address;

    loadFile($('#dynamiccontent'), "/" + page);
    $('.own-link').removeClass('active');
    this.classList.add("active");
    if (page == 'node') {
      toggleSettings(true);
    } else {
      toggleSettings(false);
    }

    hideModal(500);
  });

  $(document).on("change", '.form-range', function(event) {
    let valueLabel = $('#' + $(this).data("id") + '_label');
    valueLabel.text(this.value);
    let name = $(this).data("name");
    let origValue = $(this).data("default");
    let value = this.value;
    if (origValue != value) {
      $('button[data-name="' + name + '"]._cancel').show().removeAttr("disabled").removeClass("disabled");
      $('button[data-name="' + name + '"]._save').show();
    }
  });

  $(document).on("click", '.nav-link', function(event) {
    console.log("CLICKED");
    // reset states if coming from test site
    if (dev) {
      dev = false;
      sendDataHome("cancel", ["boardID", 255]);
    }

    if (this.id != "") {
      if (this.id == "cycle-btn") {
        let objs = window.nodes[boardID].B;
        $('.params[data-id="pumpStart"]').val(objs.s).data("default", objs.s).change();
        $('.params[data-id="pumpDuration"]').val(objs.d).data("default", objs.d).change();
        $('button[data-name="cycle"]._action').show();
      } else if (this.id == "thermometer-btn") {
        let objs = window.nodes[boardID].T;
        $('.params[data-id="thermometerDuration"]').val(objs.d).data("default", objs.d).change();
        $('button[data-name="thermometer"]._action').show();
      } else if (this.id == "lungs-btn") {
        let objs = window.nodes[boardID].L;
        $('.params[data-id="lungStart"]').val(objs.s).data("default", objs.s).change();
        $('.params[data-id="lungDuration"]').val(objs.d).data("default", objs.d).change();
        $('button[data-name="lung"]._action').show();
      }
    }
  });

  $(document).on("change", '#person', function (event) {
    if (parseInt(this.value) < 0) {
      // TODO: create modal to get a correct name
      $('#newPersonModal').modal('show');
    } else {
      $('#ranges').show();

      let objs = window.nodes[boardID].P[this.value];
      $('.params[data-id="person"]').val(this.value).data("default", this.value).change();
      $('.params[data-id="pulseStrength"]').val(objs.s).data("default", objs.s).change();
      $('.params[data-id="pulseTimeGap"]').val(objs.t).data("default", objs.t).change();
      $('.params[data-id="pulseFrequence"]').val(objs.f).data("default", objs.f).change();
      $('.params[data-id="pulseDuration"]').val(objs.d).data("default", objs.d).change();
      $('.params[data-id="pulseFlicker"]').val(objs.r).data("default", objs.r).change();
      $('button[data-name="pulse"]._action').show();
      $('button[data-name="pulse"]._save,._cancel').hide();
    }
  });

  $(document).on("click", "#saveNewPerson", function(event) {
    if ($('#person-name').val() == "xmas" || $('#person-name').val() == "luke") {
      $('#ranges').hide();
      let value = $('#person-name').val() == "xmas" ? 4711 : 4712;
      $('.params[data-id="person"]').val(value).data("default", value).change();
      $('button[data-name="pulse"]._action').show().click();
      $('button[data-name="pulse"]._save').hide();
    } else {
      $('#ranges').show();
      // TODO: save data back to config.json, reload file in background and select new element
      $('#person').append(new Option($('#person-name').val(), $('#person-name').val()));
      $('#person').val($('#person-name').val());

      $('.params[data-id="pulseStrength"]').val(30).data("default", 30).change();
      $('.params[data-id="pulseTimeGap"]').val(1).data("default", 1).change();
      $('.params[data-id="pulseFrequence"]').val(80).data("default", 80).change();
      $('button[data-name="pulse"]._save,._action').show();
    }
  });

  hideModal(500);

  // Only valid if in demo mode
  if (location.origin == "http://localhost") {
    onMessage({'data': '{"pi":false,"pR":false,"cR":false,"lR":false,"tR":false,"id":99,"m":true,"B":{"s":10,"d":60},"L":{"s":1,"d":60},"T":{"d":20},"pID":4,"r":0,"v":0,"rID":0,"P":[{"n":"Fötus","s":10,"t":1,"f":150,"d":2,"r":0},{"n":"Neugeborenes","s":20,"t":1,"f":120,"d":2,"r":0},{"n":"Kindergarten","s":30,"t":1,"f":100,"d":2,"r":0},{"n":"Jugendlich","s":30,"t":1,"f":85,"d":2,"r":0},{"n":"Erwachsen","s":30,"t":1,"f":60,"d":2,"r":0},{"n":"Senior","s":30,"t":1,"f":70,"d":2,"r":0},{"n":"Sportler","s":50,"t":1,"f":30,"d":2,"r":0},{"n":"Schwanger","s":30,"t":1,"f":80,"d":2,"r":0},{"n":"Störung","s":30,"t":1,"f":80,"d":2,"r":5}]}'});
    onMessage({'data': '{"pi":false,"pR":false,"cR":false,"lR":false,"tR":false,"id":2,"m":false,"B":{"s":10,"d":60},"L":{"s":1,"d":60},"T":{"d":20},"pID":4,"r":-45,"v":3.99,"rID":0,"P":[{"n":"Fötus","s":10,"t":1,"f":150},{"n":"Neugeborenes","s":20,"t":1,"f":120},{"n":"Kindergarten","s":30,"t":1,"f":100},{"n":"Jugendlich","s":30,"t":1,"f":85},{"n":"Erwachsen","s":30,"t":1,"f":60},{"n":"Senior","s":30,"t":1,"f":70},{"n":"Sportler","s":50,"t":1,"f":30},{"n":"Schwanger","s":30,"t":1}]}'});
    onMessage({'data': '{"pi":false,"pR":false,"cR":false,"lR":false,"tR":false,"id":1,"m":false,"B":{"s":10,"d":60},"L":{"s":1,"d":60},"T":{"d":20},"pID":4,"r":-10,"v":4.12,"rID":0,"P":[{"n":"Fötus","s":10,"t":1,"f":150},{"n":"Neugeborenes","s":20,"t":1,"f":120},{"n":"Kindergarten","s":30,"t":1,"f":100},{"n":"Jugendlich","s":30,"t":1,"f":85},{"n":"Erwachsen","s":30,"t":1,"f":60},{"n":"Senior","s":30,"t":1,"f":70},{"n":"Sportler","s":50,"t":1,"f":30},{"n":"Schwanger","s":30,"t":1}]}'});
    onMessage({'data': '{"pi":false,"pR":false,"cR":false,"lR":false,"tR":false,"id":3,"m":false,"B":{"s":10,"d":60},"L":{"s":1,"d":60},"T":{"d":20},"pID":4,"r":-80,"v":3.65,"rID":0,"P":[{"n":"Fötus","s":10,"t":1,"f":150},{"n":"Neugeborenes","s":20,"t":1,"f":120},{"n":"Kindergarten","s":30,"t":1,"f":100},{"n":"Jugendlich","s":30,"t":1,"f":85},{"n":"Erwachsen","s":30,"t":1,"f":60},{"n":"Senior","s":30,"t":1,"f":70},{"n":"Sportler","s":50,"t":1,"f":30},{"n":"Schwanger","s":30,"t":1}]}'});

    setTimeout(function() {
      onMessage({'data': '{"pi":false,"pR":false,"cR":false,"lR":false,"tR":false,"id":99,"m":true,"B":{"s":10,"d":60},"L":{"s":1,"d":60},"T":{"d":20},"pID":4,"r":0,"v":4,"rID":0,"P":[{"n":"Fötus","s":10,"t":1,"f":150},{"n":"Neugeborenes","s":20,"t":1,"f":120},{"n":"Kindergarten","s":30,"t":1,"f":100},{"n":"Jugendlich","s":30,"t":1,"f":85},{"n":"Erwachsen","s":30,"t":1,"f":60},{"n":"Senior","s":30,"t":1,"f":70},{"n":"Sportler","s":50,"t":1,"f":30},{"n":"Schwanger","s":30,"t":1}]}'});
    }, 15000);
  }
});

function onLoad(event) {
  initWebSocket();
}

function toggleSettings(flag) {
  flag ? $('.setting').show() : $('.setting').hide();
  (flag && window.nodes[boardID].m) ? $('.setting-master').show() : $('.setting-master').hide();
}

function sendDataHome(actionStr, data, timeout=500) {
  var action = actionArray[actionStr];

  $('#spinning').modal('show');
  console.log("sendDataHome", parseInt(window.boardID), actionStr, action, data);
  websocket.send(JSON.stringify({'id': parseInt(window.boardID), 'action': parseInt(action), 'data': data}));
  if (action == 0) {
    setTimeout(function() {
      location.reload();
    }, 3000);
  } else {
    setTimeout(function() {
      $('#spinning').modal('hide');
    }, timeout);
  }
}

function renderLiveData() {
  let volt = window.nodes[boardID].v; // voltage

  let batteryText = '<div style="float:right;"><svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" fill="currentColor" viewBox="0 0 16 16" class="bi bi-battery';
  let batteryPath = '';
  if (volt < 80) {
    batteryText += '" style="color:red !important;';
    batteryPath = '<path d="M0 6a2 2 0 0 1 2-2h10a2 2 0 0 1 2 2v4a2 2 0 0 1-2 2H2a2 2 0 0 1-2-2V6zm2-1a1 1 0 0 0-1 1v4a1 1 0 0 0 1 1h10a1 1 0 0 0 1-1V6a1 1 0 0 0-1-1H2zm14 3a1.5 1.5 0 0 1-1.5 1.5v-3A1.5 1.5 0 0 1 16 8z"/>';
  } else if (volt < 90) {
    batteryText += '-half" style="color:orange;';
    batteryPath = '<path d="M2 6h5v4H2V6z"/>';
    batteryPath += '<path d="M2 4a2 2 0 0 0-2 2v4a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V6a2 2 0 0 0-2-2H2zm10 1a1 1 0 0 1 1 1v4a1 1 0 0 1-1 1H2a1 1 0 0 1-1-1V6a1 1 0 0 1 1-1h10zm4 3a1.5 1.5 0 0 1-1.5 1.5v-3A1.5 1.5 0 0 1 16 8z"/>';
  } else {
    batteryText += '-full" style="color:green;';
    batteryPath = '<path d="M2 6h10v4H2V6z"/>';
    batteryPath += '<path d="M2 4a2 2 0 0 0-2 2v4a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V6a2 2 0 0 0-2-2H2zm10 1a1 1 0 0 1 1 1v4a1 1 0 0 1-1 1H2a1 1 0 0 1-1-1V6a1 1 0 0 1 1-1h10zm4 3a1.5 1.5 0 0 1-1.5 1.5v-3A1.5 1.5 0 0 1 16 8z"/>';
  }
  batteryText += 'vertical-align:sub">';

  let html = '<div style="float:left;">Board: ' + boardID + "</div>";
  html += batteryText + batteryPath + '</svg>'
  
  let wifiSymbol = "low";
  if (window.nodes[boardID].r == 0 || window.nodes[boardID].r > -30) { // 0 = only if masterBoard
    wifiSymbol = "full";
  } else if (window.nodes[boardID].r >= -0) {
    wifiSymbol = "low";
  } else if (window.nodes[boardID].r >= -80) {
    wifiSymbol = "half";
  }

  html += '&nbsp;<svg style="margin-bottom:1px;" class="bi" width="24" height="24" role="img"><use xlink:href="#wifi_' + wifiSymbol + '"></use></svg>';
  html += '</div>';
  $('#battery').html(html);

  $('._action[data-name="pulse"]').html(window.nodes[boardID].pR ? "Stop" : "Start"); 
  $('._action[data-name="cycle"]').html(window.nodes[boardID].cR ? "Stop" : "Start"); 
  $('._action[data-name="lung"]').html(window.nodes[boardID].lR ? "Stop" : "Start"); 
  $('._action[data-name="thermometer"]').html(window.nodes[boardID].tR ? "Stop" : "Start"); 
}

function hideModal(ms) {
  setTimeout(function () {
    $('#spinning').modal('hide');
  }, ms);
}

if (typeof interval !== 'undefined') {
  clearInterval(interval);
  delete interval;
  hideModal(500);
}

interval = setInterval(function(){
  for (let i in window.nodes) {
    let node = window.nodes[i];
    if (node.timer <= (Date.now() - 10000)) {
      console.log(Date.now() + " node " + node.id + "(" + node.m + ") is orphaned"); // master
      if (boardID == i) {
        console.log("redirect to homepage cause these node is no longer available");
        $('a[data-address=' + i + ']').css("opacity", "0.3").css("pointer-events", "none");
        $('a[data-page="home"]').click();
      }
    } else {
      $('a[data-address=' + i + ']').css("opacity", "").css("pointer-events", "");
    }
  }
}, 10000);
