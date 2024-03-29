const char sensors_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
	<title>Home weather station</title>
</head>
<style>

	@import url(https://fonts.googleapis.com/css?family=Montserrat);
	@import url(https://fonts.googleapis.com/css?family=Advent+Pro:400,200);
	*{margin: 0;padding: 0;}
 
	body{
	  background:#544947;
	  font-family:Montserrat,Arial,sans-serif;
	}
	h2{
	  font-size:18px;
	}
	
	.widget{
	  box-shadow:0 40px 10px 5px rgba(0,0,0,0.4);
	  margin:100px auto;
	  height: 400px;
	  position: relative;
	  width: 800px;
	} 
	.upper{
	  border-radius:5px 5px 0 0;
	  background:#f5f5f5;
	  height:200px;
	  padding:20px;
	}
 
	.date{
	  font-size:30px;
	}
	.year{
	  font-size:30px;
	  color:#c1c1c1;
	}
	.place{
	  color:#222;
	  font-size:40px;
	}
	.lower{
	  background:#be29ec;
	  border-radius:0 0 5px 5px;
	  font-family:'Advent Pro';
	  font-weight:200;
	  height:200px;
	  width:100%;
	}
	.clock{
	  background:#00A8A9;
	  border-radius:100%;
	  box-shadow:0 0 0 15px #f5f5f5,0 10px 10px 5px rgba(0,0,0,0.3);
	  height:150px;
	  position:absolute;
	  right:25px;
	  top:-35px;
	  width:150px;
	}
 
	.hour{
	  background:#f5f5f5;
	  height:50px;
	  left:50%;  
	  position: absolute;
	  top:25px;
	  width:4px;
	}
 
	.minute{
	  background:#f5f5f5;
	  height:65px;
	  left:50%;  
	  position: absolute;
	  top:10px;
	  transform:rotate(100deg);
	  width:4px;
	}
	 
	.minute,.hour{
	  border-radius:5px;
	  transform-origin:bottom center;
	  transition:all .5s linear;
	}
 
	.infos{
	  list-style:none;
	}
	.info{
	  color:#fff;
	  float:left;
	  height:100%;
	  padding-top:10px;
	  text-align:center;
	  width:25%;
	}
	.info span{
	  display: inline-block;
	  font-size:40px;
	  margin-top:20px;
	}

	.anim{animation:fade .8s linear;}
	 
	@keyframes fade{
	  0%{opacity:0;}
	  100%{opacity:1;}
	}

</style>
<body>
	<div class="widget">
		<div class="clock">
			<div class="minute" id="minute"></div>
			<div class="hour" id="hour"></div>
		</div>
		<div class="upper">
			<div class="date" id="date"></div>
			<div class="year">Temperature</div>
			<div class="place update" id="temperature2">23 &deg;C</div>
		</div>
		<div class="lower">
			<ul class="infos">
				<li class="info temperature">
					<h2 class="title">TEMPERATURE</h2>
					<span class="update" id="temperature">21 &deg;C</span>
				</li>
				<li class="info humidity">
					<h2 class="title">HUMIDITY</h2>
					<span class="update" id="humidity">23 %</span>
				</li>
				<li class="info tvoc">
					<h2 class="title">TVOC</h2>
					<span class="update" id="tvoc">0.00 ppb</span>
				</li>
				<li class="info co2">
					<h2 class="title">CO2</h2>
					<span class="update" id="co2">400.00 ppm</span>
				</li>
			</ul>
		</div>
	</div>
	
	<script>
		setInterval(updatePage, 2000);
		function updatePage() {
			var date = new Date();
			var hour = date.getHours();
			var minutes = date.getMinutes();
			var seconds = date.getSeconds();
			// date
			var options = {year: 'numeric', month: 'long', day: 'numeric'};
			var today = new Date();
			document.getElementById('date').innerHTML = today.toLocaleDateString("en-US", options);
			// hour
			var hourAngle = (360 * (hour / 12)) + ((360 / 12) * (minutes / 60));
			document.getElementById("hour").style.transform = "rotate(" + (hourAngle) + "deg)";
			// minute
			var minuteAngle = 360 * (minutes / 60);
			document.getElementById("minute").style.transform = "rotate(" + (minuteAngle) + "deg)";
			// get measurements
			var xhttp = new XMLHttpRequest();
			xhttp.onreadystatechange = function() {
				if (this.readyState == 4 && this.status == 200) {
					var txt = this.responseText;
					var obj = JSON.parse(txt);
					document.getElementById("temperature2").innerHTML = Math.round(obj.temperature) + " &deg;C";
					document.getElementById("temperature").innerHTML = Math.round(obj.temperature) + " &deg;C";
					document.getElementById("humidity").innerHTML = Math.round(obj.humidity) + " %";
					document.getElementById("tvoc").innerHTML = Math.round(obj.tvoc) + " ppb";
					document.getElementById("co2").innerHTML = Math.round(obj.co2) + " ppm";
				}
			};
			xhttp.open("GET", "measurements", true);
			xhttp.send();
		}
	</script>
</body>
</html>
)=====";