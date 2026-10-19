/*
 * post.js
 *
 * En algunas versiones de Emscripten, pedir "HEAPU8" en
 * EXPORTED_RUNTIME_METHODS no lo llega a exponer de verdad en el objeto
 * Module (bug conocido). Este post-js corre DENTRO del mismo scope que el
 * resto del código generado, así que puede ver la variable interna HEAPU8
 * directamente y engancharla a mano — funciona pase lo que pase con el
 * flag de arriba.
 *
 * Se define como getter (no como asignación única) porque con
 * ALLOW_MEMORY_GROWTH=1 el buffer de memoria puede reemplazarse por uno
 * más grande en cualquier momento; un getter siempre devuelve la vista
 * interna actual en vez de una copia que podría quedar obsoleta.
 */
Object.defineProperty(Module, 'HEAPU8', {
	configurable: true,
	get: function () { return HEAPU8; }
});
