import * as THREE from "three";
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import { GUI } from 'three/examples/jsm/libs/lil-gui.module.min.js';

import * as SPHERES from "../libs/SphereHandler.js"
import * as WALLS from "../libs/WallHandler.js"
import * as LAYOUT from '../libs/Layout.js'
// import { NDSTLLoader, renderSTL } from '../libs/NDSTLLoader.js';

var urlParams = new URLSearchParams(window.location.search);
var clock = new THREE.Clock();

let camera, scene, renderer, stats, panel, controls;
let position;
let gui;
let spheres;
let floor;
let S;
let NDDEMCGLib;
let pointer;
let v, omegaMag;
let radii;
let NDsolids, material, STLFilename;
let meshes = new THREE.Group();
let density, vavg, stressTcxx, stressTcyy, stressTczz, stressTcxy;
let pressure=[], shearstress=[], xloc=[] ;
let show_stats = true;

const raycaster = new THREE.Raycaster();
const mouse = new THREE.Vector2();
let intersection_plane = new THREE.Plane();
let camera_direction = new THREE.Vector3();

let graph_fraction = 0.5;
document.getElementById("stats").style.width = String(100*graph_fraction) + '%';
document.getElementById("canvas").style.width = String(100*(1-graph_fraction)) + '%';

var params = {
    dimension: 3,
    initial_packing_fraction: 0.6,
    // L: 0.025, //system size
    N: 500,
    // packing_fraction: 0.5,
    constant_volume: true,
    axial_strain: 0,
    volumetric_strain: 0,
    paused: false,
    g_mag: 1e3,
    theta: 0, // slope angle in DEGREES
    d4: { cur:0,
         min:0,
         max:0 },
    r_max: 0.0033,
    r_min: 0.0027,
    freq: 0.05,
    new_line: false,
    shear_rate: 10,
    lut: 'None',
    quality: 5,
    vmax: 50, // max velocity to colour by
    omegamax: 50, // max rotation rate to colour by
    particle_density: 2700,
    particle_opacity: 1.0,
    show_colorbar: true,
}

if ( urlParams.has('dimension') ) {
    params.dimension = parseInt(urlParams.get('dimension'));
}

set_derived_properties();

function set_derived_properties() {
    params.average_radius = (params.r_min + params.r_max)/2.;
    if ( params.dimension === 3 ) {
        params.particle_volume = 4./3.*Math.PI*Math.pow(params.average_radius,3);
    } else if ( params.dimension === 4 ) {
        params.particle_volume = Math.PI*Math.PI*Math.pow(params.average_radius,4)/2.;
    }

    console.log('estimate of particle volume: ' + params.particle_volume*params.N)
    params.particle_mass = params.particle_volume * params.particle_density;
    params.L = Math.pow(params.particle_volume*params.N/params.initial_packing_fraction, 1./params.dimension)/2.;
}





if ( urlParams.has('quality') ) { params.quality = parseInt(urlParams.get('quality')); }

SPHERES.createNDParticleShader(params).then( init() );

async function init() {

    await NDDEMCGPhysics();
    // physics.main(params.dimensions, params.N, inputfile)
    position = new THREE.Vector3();

    //

    camera = new THREE.PerspectiveCamera( 50, window.innerWidth*(1-graph_fraction) / window.innerHeight, 1e-5, 1000 );
    camera.position.set( 0, 0, -5*params.L );
    camera.up.set(1, 0, 0);

    scene = new THREE.Scene();
    scene.background = new THREE.Color( 0x111 );

    const hemiLight = new THREE.HemisphereLight();
    hemiLight.intensity = 0.35;
    scene.add( hemiLight );

    const dirLight = new THREE.DirectionalLight();
    dirLight.position.set( 5, -5, -5 );
    dirLight.castShadow = true;
    scene.add( dirLight );

    SPHERES.add_spheres(S,params,scene);

    renderer = new THREE.WebGLRenderer( { antialias: true } );
    renderer.setPixelRatio( window.devicePixelRatio );
    renderer.setSize( window.innerWidth*(1-graph_fraction), window.innerHeight );
    renderer.shadowMap.enabled = true;
    renderer.outputEncoding = THREE.sRGBEncoding;

    var container = document.getElementById( 'canvas' );
    container.appendChild( renderer.domElement );

    gui = new GUI();
    gui.width = 320;

    if ( params.dimension == 4 ) {
        gui.add( params.d4, 'cur', -params.L,params.L, 0.001)
            .name( 'D4 location').listen()
            .onChange( function () {
                if ( urlParams.has('stl') ) {
                    meshes = renderSTL( meshes, NDsolids, scene, material, params.d4.cur );
                }
            });
    }
    gui.add ( params, 'shear_rate', -100, 100, 0.1).name('Shear rate (1/s) (W/S)').listen()
        .onChange( () => update_shear_rate() );
    gui.add ( params, 'particle_opacity',0,1).name('Particle opacity').listen().onChange( () => SPHERES.update_particle_material(params,
        // lut_folder
    ));
    gui.add ( params, 'lut', ['None', 'Velocity', 'Fluct Velocity', 'Rotation Rate' ]).name('Colour by')
            .onChange( () => SPHERES.update_particle_material(params,
                // lut_folder
            ) );
    gui.add ( params, 'paused').name('Paused').listen();
    // gui.add ( params, 'new_line').name('New loading path').listen()
    //     .onChange( () => {
    //         var data = [{
    //                       type: 'scatter',
    //                       mode: 'lines',
    //                       x: [], y: [],
    //                       line: { width: 5 },
    //                       name: 'Load path ' + String(document.getElementById('stats').data.length+1)
    //                     }]
    //         Plotly.addTraces('stats', data);
    //         params.new_line = false;
    //     });
    controls = new OrbitControls( camera, container );
    controls.update();

    window.addEventListener( 'resize', onWindowResize, false );
    window.addEventListener( 'keypress', checkKeys, false );

    if ( show_stats ) { make_graph(); }

    animate();
}


function update_shear_rate() {
    S.simu_setBoundary(0, [-params.L,params.L,params.shear_rate]);
    params.vmax = 1.5*Math.abs(params.shear_rate)*params.L;
    params.omegamax = 1e3*Math.abs(params.shear_rate)*params.average_radius;
    SPHERES.update_particle_material(params);
}

function checkKeys( event ) {
    if ( event.code === 'KeyW' ) {
        params.shear_rate += 0.1;
        update_shear_rate();
        }
    if ( event.code === 'KeyS' ) {
        params.shear_rate -= 0.1;
        update_shear_rate();
        }
}

function onWindowResize(){

    camera.aspect = window.innerWidth*(1-graph_fraction) / window.innerHeight;
    camera.updateProjectionMatrix();

    renderer.setSize( window.innerWidth*(1-graph_fraction), window.innerHeight );

    var update = {
        width: window.innerWidth*graph_fraction,
        height: window.innerHeight
        };
    Plotly.relayout('stats', update);

}

function animate() {
    requestAnimationFrame( animate );
    if ( !params.paused ) {
        SPHERES.move_spheres(S,params);
        S.simu_step_forward(5);
        S.cg_param_read_timestep(0) ;
        S.cg_process_timestep(0,false) ;
        var grid = S.cg_get_gridinfo();
        density=S.cg_get_result(0, "RHO", 0) ;
        vavg=S.cg_get_result(0, "VAVG", 1) ;
        let normal_stresses = [];
        for (let i=0; i<params.dimension; i++) {
            let component = i*(params.dimension+1);
            let this_stress = S.cg_get_result(0, "TC", component) ;
            normal_stresses.push(this_stress);
        }
        // stressTcxx=S.cg_get_result(0, "TC", 0) ;
        // stressTcyy=S.cg_get_result(0, "TC", 4) ;
        // stressTczz=S.cg_get_result(0, "TC", 8) ;
        stressTcxy=S.cg_get_result(0, "TC", 1) ;
        for (var i=0 ; i<stressTcxy.length ; i++)
        {
            xloc[i]=grid[0]+i*grid[3] ;
            shearstress[i]=-stressTcxy[i] ;
            let this_pressure = 0;
            for ( let j=0; j<normal_stresses.length; j++) {
                this_pressure += normal_stresses[j][i];
            }
            pressure[i]=this_pressure / normal_stresses.length;
        }
        // params.viscosity = 0;
        // params.inertial_number = 0.1;
        // params.target_pressure = Math.pow(params.shear_rate*params.average_radius/params.inertial_number,2)*params.particle_density;
        // params.current_pressure = pressure.reduce((a, b) => a + b, 0) / pressure.length; // average vertical stress
        // let dt = 1e-3;
        // params.wall_mass = 1e4;//params.N*params.particle_mass;
        // WALLS.update_damped_wall(params, S, scene, dt)
        update_graph();
        SPHERES.draw_force_network(S, params, scene);
    }
    renderer.render( scene, camera );
}

async function NDDEMCGPhysics() {

    if ( 'DEMCGND' in window === false ) {

        console.error( 'NDDEMPhysics: Couldn\'t find DEMCGND.js' );
        return;

    }

    await DEMCGND().then( (NDDEMCGLib) => {
        if ( params.dimension == 3 ) {
            S = new NDDEMCGLib.DEMCG3D (params.N);
        }
        else if ( params.dimension == 4 ) {
            S = new NDDEMCGLib.DEMCG4D (params.N);
        }
        else if ( params.dimension == 5 ) {
            S = new NDDEMCGLib.DEMCG5D (params.N);
        }
        finish_setup();
    } );



    function finish_setup() {
        S.simu_interpret_command("dimensions " + String(params.dimension) + " " + String(params.N));
        S.simu_interpret_command("radius -1 0.5");
        let m = 4./3.*Math.PI*0.5*0.5*0.5*params.particle_density;
        S.simu_interpret_command("mass -1 " + String(m));
        S.simu_interpret_command("auto rho");
        S.simu_interpret_command("auto radius uniform "+params.r_min+" "+params.r_max);
        S.simu_interpret_command("auto mass");
        S.simu_interpret_command("auto inertia");
        S.simu_interpret_command("auto skin");

        S.simu_interpret_command("boundary 0 PBCLE -"+String(params.L)+" "+String(params.L)+" "+String(params.shear_rate));
        S.simu_interpret_command("boundary 1 PBC -"+String(params.L)+" "+String(params.L));
        S.simu_interpret_command("boundary 2 PBC -"+String(params.L)+" "+String(params.L));
        if ( params.dimension == 4 ) {
            S.simu_interpret_command("boundary 3 PBC -"+String(params.L)+" "+String(params.L));
        }
        S.simu_interpret_command("gravity 0 0 " + "0 ".repeat(params.dimension - 3))

        S.simu_interpret_command("auto location randomdrop");

        let tc = 1e-3;
        let rest = 0.2; // super low restitution coeff to dampen out quickly
        let vals = SPHERES.setCollisionTimeAndRestitutionCoefficient (tc, rest, params.particle_mass)

        S.simu_interpret_command("set Kn " + String(vals.stiffness));
        S.simu_interpret_command("set Kt " + String(0.8*vals.stiffness));
        S.simu_interpret_command("set GammaN " + String(vals.dissipation));
        S.simu_interpret_command("set GammaT " + String(vals.dissipation));
        S.simu_interpret_command("set Mu 0.5");
        S.simu_interpret_command("set T 150");
        S.simu_interpret_command("set dt " + String(tc/20));
        S.simu_interpret_command("set tdump 1000000"); // how often to calculate wall forces
        S.simu_finalise_init () ;

        var cgparam ={} ;
        cgparam["file"]=[{"filename":"none", "content": "particles", "format":"interactive", "number":1}] ;
        cgparam["boxes"]=Array(params.dimension).fill(1) ;
        cgparam["boxes"][0] = 20; // more in first dimension
        // cgparam["boundaries"]=[[-params.L,-params.L,-params.L],[params.L,params.L,params.L]] ;
        cgparam["boundaries"]=[
            Array(params.dimension).fill(-params.L+params.r_max),
            Array(params.dimension).fill( params.L-params.r_max)];
        cgparam["window size"]=2*params.average_radius ;
        cgparam["skip"]=0;
        cgparam["max time"]=1 ;
        cgparam["time average"]="None" ;
        cgparam["fields"]=["RHO", "VAVG", "TC"] ;
        cgparam["periodicity"]=Array(params.dimension).fill(true);
        cgparam["window"]="Lucy3D";
        cgparam["dimension"]=params.dimension;


        console.log(JSON.stringify(cgparam)) ;
        S.cg_param_from_json_string(JSON.stringify(cgparam)) ;
        S.cg_setup_CG() ;

    }
}

function update_graph() {

    var maxVelocity = vavg.reduce(function(a, b) { return Math.max(Math.abs(a), Math.abs(b)) }, 0);

    Plotly.update('stats', {
            'y': [xloc],
            // 'y': [[shear]],
            'x': [vavg],
    }, {
        xaxis: {
            range: [-maxVelocity, maxVelocity],
            title: 'Velocity (mm/s)',
        }
    }, [0])

    Plotly.update('stats', {
            'y': [xloc],
            // 'y': [[shear]],
            'x': [pressure],
    }, {}, [1])


    Plotly.update('stats', {
            'y': [xloc],
            // 'y': [[shear]],
            'x': [shearstress],
    }, {}, [2])
}

function make_graph() {
    let { data, layout } = LAYOUT.plotly_two_xaxis_graph('Average velocity (m/s)','Stress (Pa)','Location (mm)','Velocity','Pressure (p)','Shear stress (𝜏)');
    Plotly.newPlot('stats', data, layout);
}
