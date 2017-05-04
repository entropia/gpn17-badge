var encTypes = {
    5: "WEP",
    2: "TKIP",
    4: "CCMP",
    7: "OPEN",
    8: "AUTO"
};
var scanRes = null;
$(document).ready(function () {
    $('#scan-btn').on('click', function () {
        var netlist = $('#net-select');
	netlist.empty();
        $('#info-box').html('Scanning...');
        $('#scan-btn').attr("disabled");
        $('#scan-btn').addClass("disabled");
        $.getJSON('/api/scan', function (res) {
            scanRes = res;
            $.each(res, function (i, field) {
                netlist.append($('<option>', {
                    value: field.id,
                    text: (field.ssid + ' (' + encTypes[field.encType] + ', ' + field.rssi + 'dB)')
                }));
            });
            $('#scan-btn').removeAttr("disabled");
            $('#scan-btn').removeClass("disabled");
            $('#info-box').html('Scan Done. Found <strong>' + res.length + '</strong> networks.<br/>Ready.');
        });
    });
    $('#connect-btn').on('click', function () {
        $('#info-box').html("Connecting... Please wait.");
        $('#scan-btn').attr("disabled");
        $('#scan-btn').addClass("disabled");
        $('#connect-btn').attr("disabled");
        $('#connect-btn').addClass("disabled");
        $.post("/api/conf-wifi", {net: $('#net-select').val(), pw: $('#wifi-pw').val()}, function (data) {
            if (data == "true") {
                $('#info-box').html('<strong>Successfully connected!</strong> The configuration is saved and will be used.');
            } else {
                $('#info-box').html('<strong>Error connecting to selected WiFi.</strong> Please check the password.');
            }
            $('#scan-btn').removeAttr("disabled");
            $('#scan-btn').removeClass("disabled");
            $('#connect-btn').removeAttr("disabled");
            $('#connect-btn').removeClass("disabled");
        });
    });
    $('#net-select').change(function () {
        if (scanRes == null || $('#net-select').val() == '-1') {
            $('#connect-btn').attr("disabled");
            $('#connect-btn').addClass("disabled");
            $('#wifi-pw').addClass("hidden");
            return;
        }
        var val = $('#net-select').val();
        if (encTypes[scanRes[val].encType] == "OPEN") {
            $('#connect-btn').removeAttr("disabled");
            $('#connect-btn').removeClass("disabled");
            $('#wifi-pw').addClass("hidden");
        } else {
            $('#wifi-pw').removeClass("hidden");
            $('#connect-btn').attr("disabled");
            $('#connect-btn').addClass("disabled");
        }
    });
    $('#wifi-pw').on('change keyup paste', function () {
        window.console.log($('#wifi-pw').val().length);
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
