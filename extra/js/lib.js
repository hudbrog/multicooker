var chart = new CanvasJS.Chart("chartContainer",
		{
			zoomEnabled: false,
      animationEnabled: true,
			title:{
				text: "Temperature"
			},
			axisY2:{
				valueFormatString:"0",

				maximum: 150,
				interval: 10,
				interlacedColor: "#F5F5F5",
				gridColor: "#D7D7D7",
	 			tickColor: "#D7D7D7"
			},
      theme: "theme2",
      toolTip:{
              shared: true
      },
			legend:{
				verticalAlign: "bottom",
				horizontalAlign: "center",
				fontSize: 15,
				fontFamily: "Lucida Sans Unicode"
			},
			data: [
  			{
  				type: "line",
  				lineThickness:3,
  				axisYType:"secondary",
          xValueType: "dateTime",
  				showInLegend: true,
  				name: "Bottom",
  				dataPoints: [
  				]
  			},
  			{
  				type: "line",
  				lineThickness:3,
  				showInLegend: true,
  				name: "Top",
  				axisYType:"secondary",
          xValueType: "dateTime",
  				dataPoints: [
  				]
  			}
			],
      legend: {
        cursor:"pointer",
        itemclick : function(e) {
          if (typeof(e.dataSeries.visible) === "undefined" || e.dataSeries.visible) {
          e.dataSeries.visible = false;
          }
          else {
            e.dataSeries.visible = true;
          }
          chart.render();
        }
      }
    });

    mcData.forEach(function(val, i, arr){
      chart['options']['data'][0].dataPoints.push({x: val[2]*1000, y: val[0]});
      chart['options']['data'][1].dataPoints.push({x: val[2]*1000, y: val[1]});
    });

    chart.render();
