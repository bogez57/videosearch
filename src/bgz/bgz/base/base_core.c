////////////////////////////////
//~ bgz: raddebugger's Core

exported U64
NowTimeUS(void) {
   U64 result = now_time_us();
   return result;
}

exported void
SleepMS(U32 ms) {
   sleep_ms(ms);
}

exported TxtPt
TxtPtMake(S64 line, S64 column) {
   TxtPt result = txt_pt(line, column);
   return result;
}

exported B32
TxtPtMatch(TxtPt a, TxtPt b) {
   B32 result = txt_pt_match(a, b);
   return result;
}

exported B32
TxtPtLessThan(TxtPt a, TxtPt b) {
   B32 result = txt_pt_less_than(a, b);
   return result;
}

exported TxtPt
TxtPtMin(TxtPt a, TxtPt b) {
   TxtPt result = txt_pt_min(a, b);
   return result;
}

exported TxtPt
TxtPtMax(TxtPt a, TxtPt b) {
   TxtPt result = txt_pt_max(a, b);
   return result;
}

exported TxtRng
TxtRngMake(TxtPt min, TxtPt max) {
   TxtRng result = txt_rng(min, max);
   return result;
}
