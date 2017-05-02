$(function () {
	$('#scan-btn').on('click', function() {
		var netlist = $('#net-select');
		$.getJSON('/api/scan', function(res) {
			$.each(res, function(i, field) {
				netlist.append($('<option>', {value : field.ssid, text: (field.ssid + ' ('+field.rssi+')')}));
			});
		});
	});
});
