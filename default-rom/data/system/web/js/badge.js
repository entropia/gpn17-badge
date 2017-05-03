var encTypes = {
    5: "WEP",
    2: "TKIP",
    4: "CCMP",
    7: "OPEN",
    8: "AUTO"
};
$(document).ready(function () {
    $('#scan-btn').on('click', function () {
        var netlist = $('#net-select');
        $('#info-box').html('Scanning...');
        $('#scan-btn').attr("disabled");
        $('#scan-btn').addClass("disabled");
        $.getJSON('/api/scan', function (res) {
            $.each(res, function (i, field) {
                netlist.append($('<option>', {
                    value: field.ssid,
                    text: (field.ssid + ' (' + encTypes[field.encType] + ', ' + field.rssi + 'dB)')
                }));
            });
            $('#scan-btn').removeAttr("disabled");
            $('#scan-btn').removeClass("disabled");
            $('#info-box').html('Scan Done. Found <strong>' + res.length + '</strong> networks.<br/>Ready.');
        });
    });
    $('#info-box').html('Ready.');
});
