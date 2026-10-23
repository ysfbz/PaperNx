// Legacy fixture workflows wait for the asynchronous inventory. Production
// startup and the dedicated library suite exercise the non-blocking path.
static bool initReady(PaperNxApp &app) {
    if(!app.init())return false;
    const auto deadline=SDL_GetTicks()+10000;
    while(app.libraryScanner_.pending() && SDL_GetTicks()<deadline){app.processLibraryScan();SDL_Delay(2);}
    return !app.libraryScanner_.pending();
}
