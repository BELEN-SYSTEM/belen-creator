-- Contra debe guardarse hasheada (SHA-256 con salt BelenCreatorSalt2026).
-- Tras ejecutar este seed, ejecutar update_password_hash.sql para el usuario admin.
INSERT INTO usuario (permiso, nombre, correo, contra)
VALUES ('admin', 'Joan Martorell', 'joanmartorellcoll03@gmail.com', '215930b9ae7dba3800999d1dbf4eb6a9b86cefe95ff6d2bff2f6a72b9ad72b39');

INSERT INTO tipo (nombre) VALUES
    ('Edificio'), ('Figura'), ('On/Off'), ('Tira Led'),
    ('Angel'), ('Sonido'), ('Video');

INSERT INTO propietario (nombre) VALUES
    ('Joan Josep Martorell Cánaves'),
    ('Montserrat Martorell Coll'),
    ('Joan Martorell Coll');

INSERT INTO ubicacion (nombre) VALUES
    ('Caja 1'),
    ('Caja 2');
