-- ---------------------------------------------------------------------------
-- propietario, ubicacion, pieza, timeline, bluetooth (user_id)
-- ---------------------------------------------------------------------------
DROP POLICY IF EXISTS "propietario_select" ON public.propietario;
DROP POLICY IF EXISTS "propietario_insert" ON public.propietario;
DROP POLICY IF EXISTS "propietario_update" ON public.propietario;
DROP POLICY IF EXISTS "propietario_delete" ON public.propietario;

CREATE POLICY "propietario_select" ON public.propietario
  FOR SELECT TO authenticated
  USING (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()));

CREATE POLICY "propietario_insert" ON public.propietario
  FOR INSERT TO authenticated
  WITH CHECK (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()));

CREATE POLICY "propietario_update" ON public.propietario
  FOR UPDATE TO authenticated
  USING (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()))
  WITH CHECK (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()));

CREATE POLICY "propietario_delete" ON public.propietario
  FOR DELETE TO authenticated
  USING (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()));

DROP POLICY IF EXISTS "ubicacion_select" ON public.ubicacion;
DROP POLICY IF EXISTS "ubicacion_insert" ON public.ubicacion;
DROP POLICY IF EXISTS "ubicacion_update" ON public.ubicacion;
DROP POLICY IF EXISTS "ubicacion_delete" ON public.ubicacion;

CREATE POLICY "ubicacion_select" ON public.ubicacion
  FOR SELECT TO authenticated
  USING (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()));

CREATE POLICY "ubicacion_insert" ON public.ubicacion
  FOR INSERT TO authenticated
  WITH CHECK (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()));

CREATE POLICY "ubicacion_update" ON public.ubicacion
  FOR UPDATE TO authenticated
  USING (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()))
  WITH CHECK (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()));

CREATE POLICY "ubicacion_delete" ON public.ubicacion
  FOR DELETE TO authenticated
  USING (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()));

DROP POLICY IF EXISTS "pieza_select" ON public.pieza;
DROP POLICY IF EXISTS "pieza_insert" ON public.pieza;
DROP POLICY IF EXISTS "pieza_update" ON public.pieza;
DROP POLICY IF EXISTS "pieza_delete" ON public.pieza;

CREATE POLICY "pieza_select" ON public.pieza
  FOR SELECT TO authenticated
  USING (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()));

CREATE POLICY "pieza_insert" ON public.pieza
  FOR INSERT TO authenticated
  WITH CHECK (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()));

CREATE POLICY "pieza_update" ON public.pieza
  FOR UPDATE TO authenticated
  USING (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()))
  WITH CHECK (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()));

CREATE POLICY "pieza_delete" ON public.pieza
  FOR DELETE TO authenticated
  USING (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()));

DROP POLICY IF EXISTS "timeline_select" ON public.timeline;
DROP POLICY IF EXISTS "timeline_insert" ON public.timeline;
DROP POLICY IF EXISTS "timeline_update" ON public.timeline;
DROP POLICY IF EXISTS "timeline_delete" ON public.timeline;

CREATE POLICY "timeline_select" ON public.timeline
  FOR SELECT TO authenticated
  USING (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()));

CREATE POLICY "timeline_insert" ON public.timeline
  FOR INSERT TO authenticated
  WITH CHECK (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()));

CREATE POLICY "timeline_update" ON public.timeline
  FOR UPDATE TO authenticated
  USING (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()))
  WITH CHECK (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()));

CREATE POLICY "timeline_delete" ON public.timeline
  FOR DELETE TO authenticated
  USING (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()));

DROP POLICY IF EXISTS "bluetooth_select" ON public.bluetooth;
DROP POLICY IF EXISTS "bluetooth_insert" ON public.bluetooth;
DROP POLICY IF EXISTS "bluetooth_update" ON public.bluetooth;
DROP POLICY IF EXISTS "bluetooth_delete" ON public.bluetooth;

CREATE POLICY "bluetooth_select" ON public.bluetooth
  FOR SELECT TO authenticated
  USING (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()));

CREATE POLICY "bluetooth_insert" ON public.bluetooth
  FOR INSERT TO authenticated
  WITH CHECK (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()));

CREATE POLICY "bluetooth_update" ON public.bluetooth
  FOR UPDATE TO authenticated
  USING (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()))
  WITH CHECK (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()));

CREATE POLICY "bluetooth_delete" ON public.bluetooth
  FOR DELETE TO authenticated
  USING (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()));
