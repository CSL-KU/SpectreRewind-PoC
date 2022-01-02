var my_bool = true;

self.onmessage = function(msg) {
    var view = new Uint32Array(msg.data.buffer);
    var localcount = 0;
    while(my_bool) {
	localcount = localcount + 1;
	view[0] = localcount;
	// view[0] = view[0] + 1;
    }
}
