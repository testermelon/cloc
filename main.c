#include<stdlib.h>
#include<unistd.h>
#include<stdio.h>
#include<xcb/xcb.h>
#include<cairo-xcb.h>
#include<cairo.h>
#include<time.h>
#include<string.h>

xcb_atom_t get_atom_from_name(xcb_connection_t *xc, char * name){
	xcb_intern_atom_cookie_t cookie = 
		//obtain atom of the selection
		xcb_intern_atom(xc,0,strlen(name),name);
	xcb_intern_atom_reply_t *reply = NULL;
	//get reply
	reply = xcb_intern_atom_reply(xc,cookie,NULL);
	xcb_atom_t atom = reply->atom;
	printf("%s = %u0 \n",name, atom);
	return atom;
}

//get the owner window of manager selection, 
//selection name is assuming screen = 0
//TODO obtain the screen number programmatically

//obtain the window ID owning the manager selections

xcb_window_t get_manager_selection_owner(xcb_connection_t *xc,int *reterr){
	xcb_get_selection_owner_cookie_t cookie = 
		xcb_get_selection_owner(
				xc, 
				get_atom_from_name(xc,"_NET_SYSTEM_TRAY_S0"));

	xcb_get_selection_owner_reply_t  *reply = NULL;
	xcb_generic_error_t **error = NULL;

	reply = xcb_get_selection_owner_reply(xc,cookie,error);
	if(error){
		*reterr = 1;
	}

	printf("Manager Selection Owner ID = %u0\n", reply->owner);
	return reply->owner;
}

void draw_clock(
		xcb_connection_t *xc, 
		xcb_window_t win, 
		xcb_visualtype_t *visual_type,
		uint32_t size)
{

	//TODO obtain window size and draw accordingly
	
	//TODO get time
	uint32_t h;
	uint32_t m;
	{
		time_t raw_time;
		struct tm *time_struct;
		time( &raw_time );
		time_struct = localtime( &raw_time );
		h = time_struct->tm_hour;
		m = time_struct->tm_min;

		//use dummy time
		//h = 10;
		//m = 10;
	}

	double center_x = size/2.0;
	double center_y = size/2.0;

	double line_w = 0.8/16.0 * size;

	double h_arm_r = 5.0/16.0 * size;
	double m_arm_r = 7.0/16.0 * size;

	double h_arm_a = 2.0*3.14*( h/12.0 + m/60.0/12.0); 
	double m_arm_a = 2.0*3.14*(m/60.0);

	//prepare cairo
	cairo_surface_t *xc_sfc;

	//create cairo context for xcb
	xc_sfc = cairo_xcb_surface_create( xc, win, visual_type, size, size);
	cairo_t *cr = cairo_create(xc_sfc);

	cairo_set_line_width(cr,line_w);
	cairo_set_antialias(cr,CAIRO_ANTIALIAS_BEST);

	cairo_push_group(cr);
	
	//hour arm
	cairo_set_source_rgb(cr, 1.0,1.0,1.0);
	cairo_new_path(cr);
	cairo_arc(cr,center_x,center_y,h_arm_r,h_arm_a-3.14/2.0,h_arm_a-3.14/2.0);
	cairo_line_to(cr,center_x,center_y);
	cairo_arc(cr,center_x,center_y,h_arm_r/3.0,h_arm_a+3.14/2.0,h_arm_a+3.14/2.0);
	cairo_line_to(cr,center_x,center_y);
	cairo_close_path(cr);
	cairo_stroke(cr);
	//minute arm
	cairo_set_source_rgb(cr,0.0,0.8,1.0);
	cairo_new_path(cr);
	cairo_arc(cr,center_x,center_y,m_arm_r,-3.14/2.0,m_arm_a-3.14/2.0);
	cairo_stroke(cr);
	//give full round for framing 
	cairo_set_source_rgb(cr,0.4,0.3,0.4);
	cairo_new_path(cr);
	cairo_arc(cr,center_x,center_y,m_arm_r,m_arm_a-3.14/2.0,3.0*3.14/2.0);
	cairo_stroke(cr);

	cairo_pop_group_to_source(cr);
	cairo_paint(cr);
	cairo_surface_flush(xc_sfc);
}


int main(int argc, char *argv[]){

	//connect to X server
	xcb_connection_t 	*xc;
	xc = xcb_connect(NULL,NULL); //(display name, screen no)

	//get first screen? 
	xcb_screen_t 		*screen;
	screen = xcb_setup_roots_iterator( xcb_get_setup(xc) ).data;

	//obtain visualtype 
	xcb_visualtype_t *visual_type;
	//iterate over all depths
	xcb_depth_iterator_t 
		depth_iter = xcb_screen_allowed_depths_iterator(screen);
	for(;depth_iter.rem;xcb_depth_next(&depth_iter)) {
		xcb_visualtype_iterator_t 
			vis_iter = xcb_depth_visuals_iterator(depth_iter.data);
		for(;vis_iter.rem;xcb_visualtype_next(&vis_iter)) {
			if(screen->root_visual == vis_iter.data->visual_id) {
				visual_type = vis_iter.data;
				break;
			}
		}
	}
	//at this point visual_type contains the correct visual struct

	//get the owner window of manager selection, 
	//selection name is assuming screen = 0
	//TODO obtain the screen number programmatically

	xcb_window_t owner;
	int err=0;
	owner = get_manager_selection_owner(xc,&err);

	while(err){
		printf("Tray not found\n");
		sleep(1);
		owner = get_manager_selection_owner(xc,&err);
	}

	//TODO ask the dimension of the available tray window
	
	//make and show a window
	
	//gen id for window
	xcb_window_t 
		win = xcb_generate_id(xc);
	
	//create icon window
	{
		//prepare masks and values
		//use a block to force short lifetime of these vars
		uint32_t mask = 
			XCB_CW_BACK_PIXEL | 
			XCB_CW_EVENT_MASK ;
		uint32_t values[2] = {
			screen->black_pixel,
			XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY,};

		xcb_create_window(
			xc, 
			XCB_COPY_FROM_PARENT, 
			win, 
			screen->root, 
			0,0, 
			16,16, 
			0, 
			XCB_WINDOW_CLASS_INPUT_OUTPUT, 
			screen->root_visual, 
			mask, values);
	}


	//compose message of docking
	xcb_client_message_event_t msg;
	msg.response_type = XCB_CLIENT_MESSAGE; //
	msg.format = 32; // options: 8, 16, 32
	msg.sequence = 0; //TODO: check 
	msg.window = win; //icon window ID 
	msg.type = get_atom_from_name(xc,"_NET_SYSTEM_TRAY_OPCODE"); 
	msg.data.data32[0] = XCB_CURRENT_TIME;
	msg.data.data32[1] = 0; //SYSTEM_TRAY_REQUEST_DOCK
	msg.data.data32[2] = win;
	msg.data.data32[3] = 0;
	msg.data.data32[4] = 0;

	xcb_send_event(
			xc,
			0, // propagate = false
			owner,
			XCB_EVENT_MASK_NO_EVENT,
			(const char *)&msg 
			);

	xcb_flush(xc);

	//Make sure we understand the initial state of our program before mapping the window
	
	//check if we are reparented
	xcb_generic_event_t *e;
	int reparented = 0;
	printf("Wait for reparenting\n");
	while(!reparented){
		if((e = xcb_poll_for_event(xc)))
			switch (e->response_type & ~0x80){
				case XCB_REPARENT_NOTIFY:
					reparented = 1;
					printf("Reparented!\n");
					break;
				default:
					printf("...\n");
					usleep(1000);
					break;
			}
	}
	
	//show window
	xcb_map_window(xc,win);

	//loop and draw forever
	while(1){
		//draw clock every second
		draw_clock(xc,win,visual_type,16);
		xcb_flush(xc);

		sleep(1);

		//TODO on click, make another window
		//to show bigger clock and date
		//with close button
		//and "press any button to close" functionality

		//flush every loop to make sure things are drawn
		//NOTE: Things did occasionally not drawn if unlucky 
		//(needs to hide-unhide window multiple times to get it drawn sometimes)
		//
		//TODO: Handle signals so we can exit gracefully
	}

	//wrap up
	xcb_disconnect(xc);

	return 0;
}
