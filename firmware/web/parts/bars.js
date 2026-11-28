var BAR_DBM = [-75, -67, -55]; /* signal needed for bars 2, 3 and 4 */

/* WiFi signal as four bars, with the number of bars for a screen reader. */
function bars(rssi) {
  var level = 1;
  BAR_DBM.forEach(function (dbm) { if (rssi >= dbm) { level++; } });
  var e = el("span", "bars");
  e.setAttribute("role", "img");
  e.setAttribute("aria-label", "[[Signal ]]" + level + "[[ of 4]]");
  for (var i = 1; i <= 4; i++) { e.appendChild(el("b", i <= level ? "on" : "")); }
  return e;
}
