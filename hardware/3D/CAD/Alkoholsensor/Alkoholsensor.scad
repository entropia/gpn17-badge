// set quality to 20 for working with the model, 200 for the export
quality = 200;
// thickness of the walls at the thinnest wall
thickness = 1.5;
// Some Text :3
text = "GPN17";
font = "DejaVu Serif";
textsize = 5;
// some perimeters, diameter of the sensor, heigth of the plug and length of the blowing tube ( length of the mouthpiece and the exhaust will be added )
diameter = 17.3;
width = diameter + thickness * 2;
height_sens = 3;
height = 10 + thickness + 2;
length = 30;
tube_offset = -3; 
//perimeters for the mouthpiece
m_width = 13;
m_height = 7;
m_lenght = 20;
m_d = 3;

//rotate the object for 3D-printing
rotate([0,90,0])

difference(){
    union(){

        // sensor cylinder
        translate([tube_offset,0,height/2])
        cylinder(h = height_sens, d = diameter + thickness * 2, $fn = quality);
        
        // main shaft and mouth piece 2
        hull(){
            translate([length/2,0,0])
            mouthpiece2();
            cube([length,width,height],center = true);
        }
        
        //exhaust
        translate([ -length/2, diameter/2+thickness, -height/2])
        rotate([90,90,0])
        mirror([1,0,0])
        exhaust(width,height,90,thickness);
        
        //mouthpiece2
        translate([length/2,0,0])
        mouthpiece1();
        
        // Text 
        translate([ length/2-3.5,-textsize/2,-height/2])
        rotate([180,0,180])
        #linear_extrude(0.5)
        text(text = str(text), font = font, size = textsize);
    }
    union(){
        
        // sensorcylinder
        translate([tube_offset,0,0]) 
        cylinder(h = height+height/2, d = diameter, $fn = quality);
        
        //inner cube
        translate([length/2,0,0])
        cube([6,m_width-thickness*2,height-thickness*2],center = true);
        
        // main shaft
        cube([length,width-thickness*2,height-thickness*2],center = true);
    }
}
/////////////////////////////////////////////////////////////////
/////////////////// Modules go here ///////////////////
/////////////////////////////////////////////////////////////////
module mouthpiece1(){
    difference(){
        union(){
            // outer cube
            translate([m_lenght/2,0.5,-0.5])
            minkowski(){
                minkowski(){
                    minkowski(){
                        cube([m_lenght-m_d*2,m_width-m_d,m_height-m_d],center = true);
                            cylinder(h=1,d=m_d,$fn=quality);
                        }
                    rotate([90,0,0])
                    cylinder(h=1,d=m_d,$fn=quality);
                }
                rotate([0,90,0])
                cylinder(h=1,d=m_d,$fn=quality);
            }
        }
        union(){
            //inner section
            hull(){
                translate([m_lenght,0,0])
                minkowski(){
                    cube([1.1,m_width-2,m_height-1.5],center = true);
                    rotate([0,90,0])
                    cylinder(h=1,d=2,$fn=quality);            
                }
                translate([m_lenght/2,0,0])
                minkowski(){
                    cube([0.5,m_width/4,m_height/4],center = true);
                    rotate([0,90,0])
                    cylinder(h=1,d=2,$fn=quality);            
                }
            }
            hull(){
                translate([m_lenght/2,0,0])
                minkowski(){
                    cube([0.5,m_width/4,m_height/4],center = true);
                    rotate([0,90,0])
                    cylinder(h=1,d=2,$fn=quality);            
                }
                translate([2,0,0])
                cube([1,m_width-thickness*2+0.5,height-thickness*2+0.25],center = true);
                }
            cube([3,m_width*1.5,m_height*1.5],center = true); 
        }
    }
}

module mouthpiece2(){
    difference(){
        union(){
            // outer Section
            translate([m_lenght/2,0.5,-0.5])
            minkowski(){
                minkowski(){
                    minkowski(){
                        cube([m_lenght-m_d*2,m_width-m_d,m_height-m_d],center = true);
                            cylinder(h=1,d=m_d,$fn=quality);
                        }
                    rotate([90,0,0])
                    cylinder(h=1,d=m_d,$fn=quality);
                }
                rotate([0,90,0])
                cylinder(h=1,d=m_d,$fn=quality);
            }
        }
        union(){
            // cutaway
            translate([0,0,0])
            cube([6,m_width-thickness*2,height-thickness*2],center = true);
            translate([m_lenght/2+3,0,0])
            cube([m_lenght,m_width*1.5,m_height*2],center = true); 
        }
    }
}


module exhaust(w,h,a,t){
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
            translate([0,0,t])
            cylinder(r=h-t,h=w-t*2,$fn=quality);
        }
    }
    
    difference(){
        union(){
            // inner ring
           cylinder(r=t,h=w,$fn=quality);
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
