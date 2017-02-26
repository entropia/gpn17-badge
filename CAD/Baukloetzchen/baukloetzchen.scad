text = "GPN17";
font = "DejaVu Serif";
//use "baukloetzchen(x,y)" to create a single height "baukloetzchen"
color("red")
baukloetzchen(20,2);

color("red")
translate([0,-100,0])

baukloetzchen(10,2);

// the pcb-method creates a shape, which looks like the pcb
translate([0,0,0])
pcb(4,19,3);

module pcb(x = 4 , y = 4,offset = 2){
    difference(){
        union(){
            hull(){
                translate([0,0,4])
                cube([8*x-0.2,8*y/3-0.2,1.6],center = true);
                translate([0,-8*y/1.8/2-0.2,4])
                cube([8*x/2-0.2,8*y/1.8-0.2,1.6],center = true);
            }
            translate([0,((-8*y-0.2)/2)+8*offset,4])
            cube([8*x/2-0.2,8*y-0.2,1.6],center = true);
        }
        union(){
        }
    }
}

module baukloetzchen(x = 1,y = 1){
    difference(){
        union(){
            translate([0,0,1.6])
            cube([8*x-0.2,8*y-0.2,3.2],center = true);
            for(i = [0:8:8*x-1]){
                for(j = [0:8:8*y-1]){
                    translate([i+4-(8*x-0.2)/2,j+4-(8*y-0.2)/2,3.2])
                    cylinder(h=1.7,d=4.8,$fn=40);
                    translate([i+4-(8*x-0.2)/2-2,j+4-(8*y-0.2)/2-0.5,5])
                    linear_extrude(0.5)
                    text(text = str(text), font = font, size = 0.8);
                }
            }
        }
        union(){
            translate([0,0,1.59])
            cube([8*x-1.2*2,8*y-1.2*2,3.2],center = true);
        }
    }
}