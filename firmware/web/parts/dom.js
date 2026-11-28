var $ = function (id) { return document.getElementById(id); };

/* A new element with this class and text. */
function el(tag, className, text) {
  var e = document.createElement(tag);
  if (className) { e.className = className; }
  if (text !== undefined) { e.textContent = text; }
  return e;
}
