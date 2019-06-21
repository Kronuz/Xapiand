---
title: Object
---

### Object of Mixed Types

{% comment %}
```json
PUT /test/mixed_objects/doc

{
  "types": {
    "type": "human",
    "legs": 2,
    "arms": 2,
  }
}
```
---
description: Index Mixed Objects
---

```json
GET /test/mixed_objects/
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._schema.schema.types.type._type).to.equal('text');
  pm.expect(jsonData._schema.schema.types.legs._type).to.equal('integer');
  pm.expect(jsonData._schema.schema.types.arms._type).to.equal('integer');
});
```
---
description: Get Mixed Objects
---

```json
INFO /test/mixed_objects/doc
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.terms.types.arms).to.have.all.keys(['<186a0>', '<2710>', '<3e8>', '<5f5e100>', '<64>', '<f4240>', 'N\ufffd@']);
  pm.expect(jsonData.terms.types.legs).to.have.all.keys(['<186a0>', '<2710>', '<3e8>', '<5f5e100>', '<64>', '<f4240>', 'N\ufffd@']);
  pm.expect(jsonData.terms.types.type).to.have.all.keys(['Shuman']);
  pm.expect(jsonData.values).to.be.an('object').that.have.all.keys(['0', '1', '1663382011','3248593248', '3741895486']);
});
```
---
description: Info Mixed Objects
---
{% endcomment %}


### Value With Nested Object

{% comment %}
```json
PUT /test/value_object_nested/doc

{
  "types": {
    "_value": {
      "type": "human",
      "legs": 2,
      "arms": 2,
      "name": {
        "first": "John",
        "last": "Doe"
      }
    }
  }
}
```
---
description: Index value with nested object
---

```json
GET /test/value_object_nested/
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._schema.schema.types.type._type).to.equal('text');
  pm.expect(jsonData._schema.schema.types.legs._type).to.equal('integer');
  pm.expect(jsonData._schema.schema.types.arms._type).to.equal('integer');
  pm.expect(jsonData._schema.schema.types.name._type).to.equal('object');
  pm.expect(jsonData._schema.schema.types.name.first._type).to.equal('text');
  pm.expect(jsonData._schema.schema.types.name.last._type).to.equal('text');
});
```

{% endcomment %}


### Simple Object

{% comment %}
```json
PUT /test/object/doc

{
  "name": {
    "_value": {
      "first": "John",
      "middle": "R",
      "last": "Doe"
    }
  }
}
```
---
description: Index Object
---

```json
GET /test/object/._schema.schema.name
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._type).to.equal('object');
});
```
---
description: Get Object
---

```json
INFO /test/object/doc.terms.name
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.first).to.have.all.keys(['Sjohn']);
  pm.expect(jsonData.last).to.have.all.keys(['Sdoe']);
  pm.expect(jsonData.middle).to.have.all.keys(['Sr']);
});
```
---
description: Info Object
---
{% endcomment %}


### Nested Object

{% comment %}
```json
PUT /test/nested_object/doc

{
  "name": {
    "_value": {
      "first": "John",
      "middle": "R",
      "last": "Doe"
    }
  }
}
```
---
description: Index Nested Object
---

```json
GET /test/nested_object/._schema.schema.name
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._type).to.equal('object');
});
```
---
description: Get Nested Object
---

```json
INFO /test/nested_object/doc.terms.name
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.first).to.have.all.keys(['Sjohn']);
  pm.expect(jsonData.last).to.have.all.keys(['Sdoe']);
  pm.expect(jsonData.middle).to.have.all.keys(['Sr']);
});
```
---
description: Info Nested Object
---
{% endcomment %}


### Complex Object

{% comment %}
```json
PUT /test/complex_object/doc

{
  "accountNumber": 121931,
  "balance": 221.46,
  "employer": "Globoil",
  "name": {
    "firstName": "Michael",
    "lastName": "Lee"
  },
  "age": 24,
  "gender": "male",
  "contact": {
    "address": "630 Victor Road",
    "city": "Leyner",
    "state": "Indiana",
    "postcode": "61952",
    "phone": "+1 (924) 594-3216",
    "email": "michael.lee@globoil.co.uk"
  },
  "checkin": {
    "_point": {
      "_longitude": -95.63079,
      "_latitude": 31.76212
    }
  },
  "favoriteFruit": "lemon",
  "eyeColor": "blue",
  "style": {
    "_namespace": true,
    "clothing": {
      "pants": "khakis",
      "shirt": "t-shirt"
    },
    "hairstyle": "slick back"
  },
  "personality": {
    "_language": "en",
    "_value": "A lot can be assumed when you first see Michael Lee, but at the very least you will find out he is elegant and heroic. Of course he is also loyal, passionate and clever, but in a way they are lesser traits and tained by behaviors of being prejudiced as well. His elegance though, this is what he is kind of cherished for. Friends frequently count on this and his excitement in times of need. All in all, Michael has a range of flaws to deal with too. His disruptive nature and insulting nature risk ruining pleasant moods and reach all around. Fortunately his heroic nature helps lighten the blows and moods when needed.",
    "_type": "text"
  }
}
```
---
description: Index Complex Object
---

```json
GET /test/complex_object/._schema.schema
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.accountNumber._type).to.equal('integer');
  pm.expect(jsonData.balance._type).to.equal('floating');
  pm.expect(jsonData.employer._type).to.equal('text');
  pm.expect(jsonData.name._type).to.equal('object');
  pm.expect(jsonData.name.firstName._type).to.equal('text');
  pm.expect(jsonData.name.lastName._type).to.equal('text');
  pm.expect(jsonData.age._type).to.equal('integer');
  pm.expect(jsonData.gender._type).to.equal('text');
  pm.expect(jsonData.contact._type).to.equal('object');
  pm.expect(jsonData.contact.address._type).to.equal('text');
  pm.expect(jsonData.contact.city._type).to.equal('text');
  pm.expect(jsonData.contact.state._type).to.equal('text');
  pm.expect(jsonData.contact.postcode._type).to.equal('text');
  pm.expect(jsonData.contact.phone._type).to.equal('text');
  pm.expect(jsonData.contact.email._type).to.equal('text');
  pm.expect(jsonData.checkin._type).to.equal('geo');
  pm.expect(jsonData.favoriteFruit._type).to.equal('text');
  pm.expect(jsonData.eyeColor._type).to.equal('text');
  pm.expect(jsonData.style._type).to.equal('text');
  pm.expect(jsonData.personality._type).to.equal('text');
});
```
---
description: Get Complex Object
---

```json
INFO /test/complex_object/doc
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Values are valid", function() {
  var jsonData = pm.response.json();

  pm.expect(jsonData.terms).to.have.any.keys(['Zpersonality']);
  pm.expect(jsonData.terms['Zpersonality']).to.have.all.keys(['Sassum','Sbehavior', 'Sblow', 'Scherish', 'Sclever', 'Scount', 'Sdeal', 'Sdisrupt', 'Seleg', 'Sexcit', 'Sfind', 'Sflaw', 'Sfortun', 'Sfrequent', 'Sfriend', 'Shelp', 'Sheroic', 'Sinsult', 'Skind', 'Slee', 'Slesser', 'Slighten', 'Slot', 'Sloyal', 'Smichael', 'Smood', 'Snatur', 'Sneed', 'Spassion', 'Spleasant', 'Sprejud', 'Srang', 'Sreach', 'Srisk', 'Sruin', 'Stain', 'Stime', 'Strait']);
  pm.expect(jsonData.terms['contact']['address']).to.have.all.keys(['S630','Sroad', 'Svictor']);
  pm.expect(jsonData.terms['contact']['city']).to.have.all.keys(['Sleyner']);
  pm.expect(jsonData.terms['contact']['email']).to.have.all.keys(['Sco', 'Sgloboil', 'Slee', 'Smichael', 'Suk']);
  pm.expect(jsonData.terms['contact']['phone']).to.have.all.keys(['S1', 'S3216', 'S594', 'S924']);
  pm.expect(jsonData.terms['contact']['postcode']).to.have.all.keys(['S61952']);
  pm.expect(jsonData.terms['contact']['state']).to.have.all.keys(['Sindiana']);
  pm.expect(jsonData.terms['employer']).to.have.all.keys(['Sgloboil']);
  pm.expect(jsonData.terms['eyeColor']).to.have.all.keys(['Sblue']);
  pm.expect(jsonData.terms['favoriteFruit']).to.have.all.keys(['Slemon']);
  pm.expect(jsonData.terms['gender']).to.have.all.keys(['Smale']);
  pm.expect(jsonData.terms['name']['firstName']).to.have.all.keys(['Smichael']);
  pm.expect(jsonData.terms['name']['lastName']).to.have.all.keys(['Slee']);
  pm.expect(jsonData.terms['personality']).to.have.all.keys(['Sa', 'Sall', 'Salso', 'Sand', 'Sare', 'Saround', 'Sas', 'Sassumed', 'Sat', 'Sbe', 'Sbehaviors', 'Sbeing', 'Sblows', 'Sbut', 'Sby', 'Scan', 'Scherished', 'Sclever', 'Scount', 'Scourse', 'Sdeal', 'Sdisruptive', 'Selegance', 'Selegant', 'Sexcitement', 'Sfind', 'Sfirst', 'Sflaws', 'Sfor', 'Sfortunately', 'Sfrequently', 'Sfriends', 'Shas', 'She', 'Shelps', 'Sheroic', 'Shis', 'Sin', 'Sinsulting', 'Sis', 'Skind', 'Sleast', 'Slee', 'Slesser', 'Slighten', 'Slot', 'Sloyal', 'Smichael', 'Smoods', 'Snature', 'Sneed', 'Sneeded', 'Sof', 'Son', 'Sout', 'Spassionate', 'Spleasant', 'Sprejudiced', 'Srange', 'Sreach', 'Srisk', 'Sruining', 'Ssee', 'Stained', 'Sthe', 'Sthey', 'Sthis', 'Sthough', 'Stimes', 'Sto', 'Stoo', 'Straits', 'Svery', 'Sway', 'Swell', 'Swhat', 'Swhen', 'Swill', 'Swith', 'Syou']);
  pm.expect(jsonData.terms['style']['clothing']['pants']).to.have.all.keys(['Skhakis']);
  pm.expect(jsonData.terms['style']['clothing']['shirt']).to.have.all.keys(['Sshirt', 'St']);
  pm.expect(jsonData.terms['style']['hairstyle']).to.have.all.keys(['Sback', 'Sslick']);
  pm.expect(jsonData.terms['style']['pants']).to.have.all.keys(['Skhakis']);
  pm.expect(jsonData.terms['style']['shirt']).to.have.all.keys(['Sshirt', 'St']);

  var t1 = '\u004e\ufffd\u0061\ufffd';
  var t2 = '\u004e\ufffd\u0075\u0030';
  var t3 = '\u004e\ufffd\u0076\u002a';
  var t4 = '\u004e\ufffd';
  var t5 = '\u004e\ufffd\u0077\u000b';
  var t6 = '\u004e\ufffd';
  var t7 = '\u004e\ufffdw\u0012\ufffd';

  pm.expect(jsonData.terms.accountNumber).to.have.all.keys(['<186a0>', '<2710>', '<3e8>', '<5f5e100>', '<64>', '<f4240>', t7]);
  pm.expect(jsonData.terms.accountNumber['<186a0>']).to.have.any.keys([t1]);
  pm.expect(jsonData.terms.accountNumber['<2710>']).to.have.any.keys([t2]);
  pm.expect(jsonData.terms.accountNumber['<3e8>']).to.have.any.keys([t3]);
  pm.expect(jsonData.terms.accountNumber['<5f5e100>']).to.have.any.keys([t4]);
  pm.expect(jsonData.terms.accountNumber['<64>']).to.have.any.keys([t5]);
  pm.expect(jsonData.terms.accountNumber['<f4240>']).to.have.any.keys([t6]);

  t1 = '\u004e\ufffd';
  t2 = '\u004e\ufffd';
  t3 = '\u004e\ufffd';
  t4 = '\u004e\ufffd';
  t5 = '\u004e\ufffd';
  t6 = '\u004e\ufffd';
  t7 = '\u004e\ufffd\u0020';

  pm.expect(jsonData.terms.age).to.have.all.keys(['<186a0>', '<2710>', '<3e8>', '<5f5e100>', '<64>', '<f4240>', t7]);
  pm.expect(jsonData.terms.age['<186a0>']).to.have.any.keys([t1]);
  pm.expect(jsonData.terms.age['<2710>']).to.have.any.keys([t2]);
  pm.expect(jsonData.terms.age['<3e8>']).to.have.any.keys([t3]);
  pm.expect(jsonData.terms.age['<5f5e100>']).to.have.any.keys([t4]);
  pm.expect(jsonData.terms.age['<64>']).to.have.any.keys([t5]);
  pm.expect(jsonData.terms.age['<f4240>']).to.have.any.keys([t6]);

  t1 = '\u004e\ufffd';
  t2 = '\u004e\ufffd';
  t3 = '\u004e\ufffd';
  t4 = '\u004e\ufffd';
  t5 = '\u004e\ufffd\u0024';
  t6 = '\u004e\ufffd';
  t7 = '\u004e\ufffd';
  var t8 = '\ufffd\ufffd\u0047\ufffd\u0014\u007c';

  pm.expect(jsonData.terms.balance).to.have.all.keys(['<186a0>', '<2710>', '<3e8>', '<5f5e100>', '<64>', '<f4240>', t7]);
  pm.expect(jsonData.terms.balance['<186a0>']).to.have.any.keys([t1]);
  pm.expect(jsonData.terms.balance['<2710>']).to.have.any.keys([t2]);
  pm.expect(jsonData.terms.balance['<3e8>']).to.have.any.keys([t3]);
  pm.expect(jsonData.terms.balance['<5f5e100>']).to.have.any.keys([t4]);
  pm.expect(jsonData.terms.balance['<64>']).to.have.any.keys([t5]);
  pm.expect(jsonData.terms.balance['<f4240>']).to.have.any.keys([t6]);
  pm.expect(jsonData.terms.balance[t7]).to.have.any.keys([t8]);

  t1 = '\u0047\ufffd\ufffd\ufffd';
  t2 = '\u0047\ufffd\ufffd\ufffd';
  t3 = '\u0047\ufffd\u0008\ufffd\u0040';
  t4 = '\u0047\ufffd\u0008\ufffd\u0047';
  t5 = '\u0047\ufffd\u0008\ufffd\u0047\u0028';
  t6 = '\u0047\u0188\ufffd\u0047\u0028\u0060';

  pm.expect(jsonData.terms.checkin).to.have.all.keys(['<3>', '<5>', '<8>', '<a>', '<c>', '<f>', 'G\u0000']);
  pm.expect(jsonData.terms.checkin['<3>']).to.have.any.keys([t1]);
  pm.expect(jsonData.terms.checkin['<5>']).to.have.any.keys([t2]);
  pm.expect(jsonData.terms.checkin['<8>']).to.have.any.keys([t3]);
  pm.expect(jsonData.terms.checkin['<a>']).to.have.any.keys([t4]);
  pm.expect(jsonData.terms.checkin['<c>']).to.have.any.keys([t5]);
  pm.expect(jsonData.terms.checkin['<f>']).to.have.any.keys([t6]);

});
```
---
description: Info Complex Object
---
{% endcomment %}