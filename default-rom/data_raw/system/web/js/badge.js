var encTypes = {
    5: "WEP",
    2: "TKIP",
    4: "CCMP",
    7: "OPEN",
    8: "AUTO"
};
var scanRes = null;
var locked = false;
$(document).ready(function () {
    $('#wifi-pw').hide();
    $('#scan-btn').on('click', function () {
        if (locked) {
            return;
        }
        lock();
        var netlist = $('#net-select');
        netlist.empty();
        $('#info-box').html('Scanning...');
        $('#scan-btn').attr("disabled");
        $('#scan-btn').addClass("disabled");
        $.getJSON('/api/scan', function (res) {
            scanRes = res;
            netlist.append($('<option>', {
                value: -1,
                text: 'No network selected...'
            }));
            $.each(res, function (i, field) {
                netlist.append($('<option>', {
                    value: field.id,
                    text: (field.ssid + ' (' + encTypes[field.encType] + ', ' + field.rssi + 'dB)')
                }));
            });
            $('#scan-btn').removeAttr("disabled");
            $('#scan-btn').removeClass("disabled");
            $('#info-box').html('Scan Done. Found <strong>' + res.length + '</strong> networks.<br/>Ready.');
            unlock();
            netlist.change();
        });
    });
    $('#connect-btn').on('click', function () {
        if (locked) {
            return;
        }
        lock();
        $('#info-box').html("Connecting... Please wait.");
        $.post("/api/conf/wifi", {net: $('#net-select').val(), pw: $('#wifi-pw').val()}, function (data) {
            if (data == "true") {
                $('#info-box').html('<strong>Successfully connected!</strong> The configuration is saved and will be used.');
            } else {
                $('#info-box').html('<strong>Error connecting to selected WiFi.</strong> Please check the password.');
            }
            unlock();
        });
    });
    $('#name-save-btn').on('click', function () {
        if (locked) {
            return;
        }
        lock();
        $.post("/api/conf/nick", {nick: $('#net-nick').val()}, function (data) {
            alert(data);
        });
        unlock();
    });
    $('#net-select').change(function () {
        if (scanRes == null || $('#net-select').val() == '-1') {
            $('#wifi-pw').hide();
            $('#connect-btn').attr("disabled");
            $('#connect-btn').addClass("disabled");
            return;
        }
        if (encTypes[scanRes[$('#net-select').val()].encType] == "OPEN") {
            $('#wifi-pw').hide();
            $('#connect-btn').removeAttr("disabled");
            $('#connect-btn').removeClass("disabled");
        } else {
            $('#wifi-pw').val("");
            $('#wifi-pw').show();
        }
        $('#wifi-pw').change();
    });
    $('#wifi-pw').on('change keyup paste', function () {
        window.console.log($('#wifi-pw').val().length);
        if ($('#net-select').val() == -1)
            return;
        if ($('#wifi-pw').val().length > 0) {
            $('#connect-btn').removeAttr("disabled");
            $('#connect-btn').removeClass("disabled");
        } else if (encTypes[scanRes[$('#net-select').val()].encType] == "OPEN") {
            return;
        } else {
            $('#connect-btn').attr("disabled");
            $('#connect-btn').addClass("disabled");
        }
    });
    $('#info-box').html('Ready.');
});
function lock() {
    locked = true;
    var elems = $(".badgeconfinput");
    elems.attr("disabled");
    elems.addClass("disabled")
}

function unlock() {
    locked = false;
    var elems = $(".badgeconfinput");
    elems.removeAttr("disabled");
    elems.removeClass("disabled");
}