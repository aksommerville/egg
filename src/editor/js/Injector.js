/* Injector.js
 * Generic dependency injection.
 */
 
export class Injector {
  static getDependencies() {
    // This won't be used; just setting a good example.
    return [Window, Document];
  }
  constructor(window, document) {
    this.window = window;
    this.document = document;
    this.singletons = {
      Window: this.window,
      Document: this.document,
      Injector: this,
    };
    this.inProgress = [];
    this.nextNonce = 1;
  }
  
  getInstance(clazz, overrides) {
    //console.log(`Injector.getInstance ${clazz} ${new Error().stack}`);
    if (clazz === "nonce") return (this.nextNonce++).toString();
    let instance = this.singletons[clazz.name];
    if (instance) return instance;
    if (this.inProgress.includes(clazz.name)) {
      throw new Error(`Dependency cycle involving these classes: ${JSON.stringify(this.inProgress)}`);
    }
    this.inProgress.push(clazz.name);
    try {
      if (instance = overrides?.find(o => o instanceof clazz)) {
        // Got from overrides, we're done.
      } else {
        const deps = this.instantiateDependencies(clazz, overrides);
        instance = new clazz(...deps);
        if (clazz.singleton) this.singletons[clazz.name] = instance;
      }
    } finally {
      const p = this.inProgress.indexOf(clazz.name);
      if (p >= 0) this.inProgress.splice(p, 1);
    }
    return instance;
  }
  
  instantiateDependencies(clazz, overrides) {
    const depClasses = clazz.getDependencies?.() || [];
    return depClasses.map(c => this.getInstance(c, overrides));
  }
}

Injector.singleton = true;
