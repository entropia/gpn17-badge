// set quality to 20 for working with the model, 200 for the export
quality = 100;
// thickness of the walls at the thinnest wall
thickness = 1.5;
// some perimeters, diameter of the sensor, heigth of the plug and length of the blowing tube ( length of the mouthpiece and the exhaust will be added )
diameter = 17;
height = 10;
cubelength = 40;

//rotate the object for 3D-printing
rotate([0,90,0])

difference(){
    union(){
        // sensor cylinder
        cylinder(h = height, d = diameter + thickness * 2, $fn = quality);
        // hull for smoothing
        hull(){
            // main shaft
            translate([0,0,-height/2])
            cube([cubelength,diameter + thickness * 2 ,height],center = true);
            // mouthpiece, minkowski curves to prevent sharp edges
            translate([cubelength/2,0.5,-height/2-0.5])
            minkowski(){
                minkowski(){
                    minkowski(){
                        cube([cubelength/2,diameter + thickness * 2,height+thickness],center = true);
                            cylinder(h=1,d=diameter/10,$fn=quality);
                        }
                    rotate([90,0,0])
                    cylinder(h=1,d=diameter/10,$fn=quality);
                }
                rotate([0,90,0])
                cylinder(h=1,d=diameter/10,$fn=quality);
            }
        }
        translate([ -cubelength/2, diameter/2+thickness, -height])
        rotate([90,90,0])
        mirror([1,0,0])
        exhaust(diameter+thickness*2,height,90);        
    }
    union(){
        // sensorcylinder
        translate([0,0,-height/2])
        cylinder(h = height+height/2, d = diameter, $fn = quality);
        hull(){
            // main shaft
            translate([0,0,-height/2])
            cube([cubelength,diameter,height - thickness],center = true);
            // mouthpiece
            translate([cubelength/2,0,-height/2])
            minkowski(){
                cube([cubelength/2+4,diameter ,height],center = true);
                rotate([0,90,0])
                cylinder(h=1,d=diameter/10,$fn=quality);
            }
        }
    }
}

module exhaust(w,h,a){
    difference(){
        union(){
            // outer ring
            cylinder(r=h,h=w,$fn=quality);
        }
        union(){
            // cut sections
            translate([-h,0,0])
            cube([h*2,h,w]);
            translate([0,h,0])
            rotate([0,0,270-a])
            cube([h,h*2,w]);
            // hollow section
            translate([0,0,thickness])
            cylinder(r=h-thickness/2,h=w-thickness*2,$fn=quality);
        }
    }
    difference(){
        union(){
            // inner ring
           cylinder(r=thickness,h=w,$fn=quality);
        }
        union(){
            // cutting sections            
            translate([-h,0,0])
            cube([h*2,h,w]);
            translate([0,h,0])
            rotate([0,0,270-a])
            cube([h,h*2,w]);  
        }
    }
}
