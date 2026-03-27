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
-- usuario
-- ---------------------------------------------------------------------------
DROP POLICY IF EXISTS "usuario_select" ON public.usuario;
DROP POLICY IF EXISTS "usuario_insert" ON public.usuario;
DROP POLICY IF EXISTS "usuario_update" ON public.usuario;
DROP POLICY IF EXISTS "usuario_delete" ON public.usuario;

CREATE POLICY "usuario_select" ON public.usuario
  FOR SELECT TO authenticated
  USING (public.is_admin() OR id = auth.uid());

CREATE POLICY "usuario_insert" ON public.usuario
  FOR INSERT TO authenticated
  WITH CHECK (public.is_admin());

CREATE POLICY "usuario_update" ON public.usuario
  FOR UPDATE TO authenticated
  USING (public.is_admin() OR id = auth.uid())
  WITH CHECK (public.is_admin() OR id = auth.uid());

CREATE POLICY "usuario_delete" ON public.usuario
  FOR DELETE TO authenticated
  USING (public.is_admin() OR id = auth.uid());

-- ---------------------------------------------------------------------------
-- tipo: solo lectura para 'user'; escritura solo admin
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
-- puerto: solo lectura para 'user'; escritura solo admin
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
