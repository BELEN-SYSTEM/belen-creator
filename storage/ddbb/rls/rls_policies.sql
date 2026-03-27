-- RLS Belen Creator
-- admin (permiso='admin') = acceso completo a todas las tablas.
-- user  (permiso='user')  = acceso completo a todas las tablas (todas las filas)
--   EXCEPTO tipo y puerto donde solo puede hacer SELECT.
-- Requiere: schema/migration.sql aplicado (tablas + is_admin() + is_app_user()).

CREATE OR REPLACE FUNCTION public.is_app_user()
RETURNS boolean
LANGUAGE sql
STABLE
SECURITY DEFINER
SET search_path = public
AS $$
  SELECT EXISTS (
    SELECT 1 FROM public.usuario
    WHERE id = auth.uid() AND permiso = 'user'
  );
$$;

-- ---------------------------------------------------------------------------
-- tipo: solo SELECT para 'user'; CRUD completo para admin
-- ---------------------------------------------------------------------------
DROP POLICY IF EXISTS "tipo_select" ON public.tipo;
DROP POLICY IF EXISTS "tipo_insert" ON public.tipo;
DROP POLICY IF EXISTS "tipo_update" ON public.tipo;
DROP POLICY IF EXISTS "tipo_delete" ON public.tipo;

CREATE POLICY "tipo_select" ON public.tipo
  FOR SELECT TO authenticated
  USING (public.is_admin() OR public.is_app_user());

CREATE POLICY "tipo_insert" ON public.tipo
  FOR INSERT TO authenticated
  WITH CHECK (public.is_admin());

CREATE POLICY "tipo_update" ON public.tipo
  FOR UPDATE TO authenticated
  USING (public.is_admin())
  WITH CHECK (public.is_admin());

CREATE POLICY "tipo_delete" ON public.tipo
  FOR DELETE TO authenticated
  USING (public.is_admin());

-- ---------------------------------------------------------------------------
-- puerto: solo SELECT para 'user'; CRUD completo para admin
-- ---------------------------------------------------------------------------
DROP POLICY IF EXISTS "puerto_select" ON public.puerto;
DROP POLICY IF EXISTS "puerto_insert" ON public.puerto;
DROP POLICY IF EXISTS "puerto_update" ON public.puerto;
DROP POLICY IF EXISTS "puerto_delete" ON public.puerto;

CREATE POLICY "puerto_select" ON public.puerto
  FOR SELECT TO authenticated
  USING (public.is_admin() OR public.is_app_user());

CREATE POLICY "puerto_insert" ON public.puerto
  FOR INSERT TO authenticated
  WITH CHECK (public.is_admin());

CREATE POLICY "puerto_update" ON public.puerto
  FOR UPDATE TO authenticated
  USING (public.is_admin())
  WITH CHECK (public.is_admin());

CREATE POLICY "puerto_delete" ON public.puerto
  FOR DELETE TO authenticated
  USING (public.is_admin());

-- ---------------------------------------------------------------------------
-- Resto de tablas: CRUD completo para admin y user (todas las filas)
-- usuario, propietario, ubicacion, pieza, pieza_tipo, galeria,
-- timeline, bluetooth, usada
-- ---------------------------------------------------------------------------

-- usuario
DROP POLICY IF EXISTS "usuario_select" ON public.usuario;
DROP POLICY IF EXISTS "usuario_insert" ON public.usuario;
DROP POLICY IF EXISTS "usuario_update" ON public.usuario;
DROP POLICY IF EXISTS "usuario_delete" ON public.usuario;

CREATE POLICY "usuario_select" ON public.usuario
  FOR SELECT TO authenticated
  USING (public.is_admin() OR public.is_app_user());

CREATE POLICY "usuario_insert" ON public.usuario
  FOR INSERT TO authenticated
  WITH CHECK (public.is_admin() OR public.is_app_user());

CREATE POLICY "usuario_update" ON public.usuario
  FOR UPDATE TO authenticated
  USING (public.is_admin() OR public.is_app_user())
  WITH CHECK (public.is_admin() OR public.is_app_user());

CREATE POLICY "usuario_delete" ON public.usuario
  FOR DELETE TO authenticated
  USING (public.is_admin() OR public.is_app_user());

-- propietario
DROP POLICY IF EXISTS "propietario_select" ON public.propietario;
DROP POLICY IF EXISTS "propietario_insert" ON public.propietario;
DROP POLICY IF EXISTS "propietario_update" ON public.propietario;
DROP POLICY IF EXISTS "propietario_delete" ON public.propietario;

CREATE POLICY "propietario_select" ON public.propietario
  FOR SELECT TO authenticated
  USING (public.is_admin() OR public.is_app_user());

CREATE POLICY "propietario_insert" ON public.propietario
  FOR INSERT TO authenticated
  WITH CHECK (public.is_admin() OR public.is_app_user());

CREATE POLICY "propietario_update" ON public.propietario
  FOR UPDATE TO authenticated
  USING (public.is_admin() OR public.is_app_user())
  WITH CHECK (public.is_admin() OR public.is_app_user());

CREATE POLICY "propietario_delete" ON public.propietario
  FOR DELETE TO authenticated
  USING (public.is_admin() OR public.is_app_user());

-- ubicacion
DROP POLICY IF EXISTS "ubicacion_select" ON public.ubicacion;
DROP POLICY IF EXISTS "ubicacion_insert" ON public.ubicacion;
DROP POLICY IF EXISTS "ubicacion_update" ON public.ubicacion;
DROP POLICY IF EXISTS "ubicacion_delete" ON public.ubicacion;

CREATE POLICY "ubicacion_select" ON public.ubicacion
  FOR SELECT TO authenticated
  USING (public.is_admin() OR public.is_app_user());

CREATE POLICY "ubicacion_insert" ON public.ubicacion
  FOR INSERT TO authenticated
  WITH CHECK (public.is_admin() OR public.is_app_user());

CREATE POLICY "ubicacion_update" ON public.ubicacion
  FOR UPDATE TO authenticated
  USING (public.is_admin() OR public.is_app_user())
  WITH CHECK (public.is_admin() OR public.is_app_user());

CREATE POLICY "ubicacion_delete" ON public.ubicacion
  FOR DELETE TO authenticated
  USING (public.is_admin() OR public.is_app_user());

-- pieza
DROP POLICY IF EXISTS "pieza_select" ON public.pieza;
DROP POLICY IF EXISTS "pieza_insert" ON public.pieza;
DROP POLICY IF EXISTS "pieza_update" ON public.pieza;
DROP POLICY IF EXISTS "pieza_delete" ON public.pieza;

CREATE POLICY "pieza_select" ON public.pieza
  FOR SELECT TO authenticated
  USING (public.is_admin() OR public.is_app_user());

CREATE POLICY "pieza_insert" ON public.pieza
  FOR INSERT TO authenticated
  WITH CHECK (public.is_admin() OR public.is_app_user());

CREATE POLICY "pieza_update" ON public.pieza
  FOR UPDATE TO authenticated
  USING (public.is_admin() OR public.is_app_user())
  WITH CHECK (public.is_admin() OR public.is_app_user());

CREATE POLICY "pieza_delete" ON public.pieza
  FOR DELETE TO authenticated
  USING (public.is_admin() OR public.is_app_user());

-- pieza_tipo
DROP POLICY IF EXISTS "pieza_tipo_select" ON public.pieza_tipo;
DROP POLICY IF EXISTS "pieza_tipo_insert" ON public.pieza_tipo;
DROP POLICY IF EXISTS "pieza_tipo_update" ON public.pieza_tipo;
DROP POLICY IF EXISTS "pieza_tipo_delete" ON public.pieza_tipo;

CREATE POLICY "pieza_tipo_select" ON public.pieza_tipo
  FOR SELECT TO authenticated
  USING (public.is_admin() OR public.is_app_user());

CREATE POLICY "pieza_tipo_insert" ON public.pieza_tipo
  FOR INSERT TO authenticated
  WITH CHECK (public.is_admin() OR public.is_app_user());

CREATE POLICY "pieza_tipo_update" ON public.pieza_tipo
  FOR UPDATE TO authenticated
  USING (public.is_admin() OR public.is_app_user())
  WITH CHECK (public.is_admin() OR public.is_app_user());

CREATE POLICY "pieza_tipo_delete" ON public.pieza_tipo
  FOR DELETE TO authenticated
  USING (public.is_admin() OR public.is_app_user());

-- galeria
DROP POLICY IF EXISTS "galeria_select" ON public.galeria;
DROP POLICY IF EXISTS "galeria_insert" ON public.galeria;
DROP POLICY IF EXISTS "galeria_update" ON public.galeria;
DROP POLICY IF EXISTS "galeria_delete" ON public.galeria;

CREATE POLICY "galeria_select" ON public.galeria
  FOR SELECT TO authenticated
  USING (public.is_admin() OR public.is_app_user());

CREATE POLICY "galeria_insert" ON public.galeria
  FOR INSERT TO authenticated
  WITH CHECK (public.is_admin() OR public.is_app_user());

CREATE POLICY "galeria_update" ON public.galeria
  FOR UPDATE TO authenticated
  USING (public.is_admin() OR public.is_app_user())
  WITH CHECK (public.is_admin() OR public.is_app_user());

CREATE POLICY "galeria_delete" ON public.galeria
  FOR DELETE TO authenticated
  USING (public.is_admin() OR public.is_app_user());

-- timeline
DROP POLICY IF EXISTS "timeline_select" ON public.timeline;
DROP POLICY IF EXISTS "timeline_insert" ON public.timeline;
DROP POLICY IF EXISTS "timeline_update" ON public.timeline;
DROP POLICY IF EXISTS "timeline_delete" ON public.timeline;

CREATE POLICY "timeline_select" ON public.timeline
  FOR SELECT TO authenticated
  USING (public.is_admin() OR public.is_app_user());

CREATE POLICY "timeline_insert" ON public.timeline
  FOR INSERT TO authenticated
  WITH CHECK (public.is_admin() OR public.is_app_user());

CREATE POLICY "timeline_update" ON public.timeline
  FOR UPDATE TO authenticated
  USING (public.is_admin() OR public.is_app_user())
  WITH CHECK (public.is_admin() OR public.is_app_user());

CREATE POLICY "timeline_delete" ON public.timeline
  FOR DELETE TO authenticated
  USING (public.is_admin() OR public.is_app_user());

-- bluetooth
DROP POLICY IF EXISTS "bluetooth_select" ON public.bluetooth;
DROP POLICY IF EXISTS "bluetooth_insert" ON public.bluetooth;
DROP POLICY IF EXISTS "bluetooth_update" ON public.bluetooth;
DROP POLICY IF EXISTS "bluetooth_delete" ON public.bluetooth;

CREATE POLICY "bluetooth_select" ON public.bluetooth
  FOR SELECT TO authenticated
  USING (public.is_admin() OR public.is_app_user());

CREATE POLICY "bluetooth_insert" ON public.bluetooth
  FOR INSERT TO authenticated
  WITH CHECK (public.is_admin() OR public.is_app_user());

CREATE POLICY "bluetooth_update" ON public.bluetooth
  FOR UPDATE TO authenticated
  USING (public.is_admin() OR public.is_app_user())
  WITH CHECK (public.is_admin() OR public.is_app_user());

CREATE POLICY "bluetooth_delete" ON public.bluetooth
  FOR DELETE TO authenticated
  USING (public.is_admin() OR public.is_app_user());

-- usada
DROP POLICY IF EXISTS "usada_select" ON public.usada;
DROP POLICY IF EXISTS "usada_insert" ON public.usada;
DROP POLICY IF EXISTS "usada_update" ON public.usada;
DROP POLICY IF EXISTS "usada_delete" ON public.usada;

CREATE POLICY "usada_select" ON public.usada
  FOR SELECT TO authenticated
  USING (public.is_admin() OR public.is_app_user());

CREATE POLICY "usada_insert" ON public.usada
  FOR INSERT TO authenticated
  WITH CHECK (public.is_admin() OR public.is_app_user());

CREATE POLICY "usada_update" ON public.usada
  FOR UPDATE TO authenticated
  USING (public.is_admin() OR public.is_app_user())
  WITH CHECK (public.is_admin() OR public.is_app_user());

CREATE POLICY "usada_delete" ON public.usada
  FOR DELETE TO authenticated
  USING (public.is_admin() OR public.is_app_user());
